#include "ford_sync_gen2_ilp_channel.h"
#include "ford_sync_gen2_ilp_signals.h"
#include "ford_sync_gen2_vmcu_peer.h"
#include <cerfcore/boards/board_context.h>
#include "ford_sync_gen2_family_id.h"
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/state/state_stream.h>
#include <algorithm>
#include <utility>

namespace cerfcore {

REGISTER_SERVICE(FordSyncGen2IlpChannel);

namespace {
using cerf::le::Put16;
using cerf::le::U16;
using cerf::le::U32;
using cerf::le::UN;
}

bool FordSyncGen2IlpChannel::ShouldRegister() {
    auto* board = emu_.TryGet<BoardContext>();
    return board && board->GetBoardFamilyId() == BoardFamilyId::FordSyncGen2;
}

bool FordSyncGen2IlpChannel::DecodeSet(const uint8_t* data, std::size_t n,
                                    std::vector<Write>& writes, ParseError& error) {
    writes.clear();
    error = {};
    auto fail = [&](const char* reason, std::size_t offset, uint32_t id = 0) {
        error = {reason, offset, id}; writes.clear(); return false;
    };
    /* EA5T-14D544-BA.sec, ipc_ilprot.dll sub_C08DD274: SetSignalsAssoc builder. */
    if (n < 6 || n > 0x3F || data[0] != 2 || data[1] != 0) return fail("header", 0);
    const unsigned count = U16(data, 4);
    std::size_t offset = 6;
    for (unsigned i = 0; i < count; ++i) {
        if (n - offset < 4) return fail("truncated identifier", offset);
        const uint32_t id = U32(data, offset);
        const auto width = FordSyncGen2IlpSignals::HeadWriteWidth(id);
        if (width == 0 || width > sizeof(uint64_t)) return fail("unknown width", offset, id);
        if (n - offset - 4 < width) return fail("truncated value", offset, id);
        writes.push_back({id, UN(data + offset + 4, width)});
        offset += 4 + width;
    }
    if (offset != n) return fail("trailing bytes", offset);
    return true;
}

void FordSyncGen2IlpChannel::Send(const uint8_t* data, std::size_t n) {
    emu_.Get<FordSyncGen2VmcuPeer>().InjectReliable(kCid, tx_seq_, data, n);
    ++sent_;
    tx_seq_ = static_cast<uint8_t>((tx_seq_ + 1) & 0x7F);
}

void FordSyncGen2IlpChannel::Complete(uint8_t type, uint16_t tid, bool accepted) {
    /* EA5T-14D544-BA.sec, ipc_ilprot.dll sub_C08DC334, sub_C08DD9A0:
       the guest treats any nonzero Set completion as failure. CERF uses byte 1
       as a generic failure; no named physical VMCU rejection code is established.
       This application completion is separate from the transport ACK and status indications. */
    uint8_t reply[6] = {static_cast<uint8_t>(type | 0x80),
        static_cast<uint8_t>(accepted ? 0 : 1)};
    Put16(reply + 2, tid);
    Send(reply, type == 4 ? 6 : 4);
}

void FordSyncGen2IlpChannel::HandleSet(const uint8_t* data, std::size_t n, uint16_t tid) {
    std::vector<Write> writes;
    ParseError error;
    if (!DecodeSet(data, n, writes, error)) {
        ++malformed_;
        LOG(Caution, "[VMCU] ILP rejected tid=%u: %s offset=%zu remaining=%zu id=0x%08X\n",
            tid, error.reason, error.offset, n - error.offset, error.id);
        Complete(2, tid, false);
        return;
    }
    const Device* owner = nullptr;
    bool unsupported = false;
    for (const auto& write : writes) {
        const Device* target = nullptr;
        for (const auto& device : devices_) {
            if (!device.owns || !device.owns(write.id)) continue;
            if (target) { unsupported = true; break; }
            target = &device;
        }
        if (!target || !target->apply || (owner && owner != target)) unsupported = true;
        if (target) owner = target;
        if (!target || logged_writes_ < 512) {
            if (logged_writes_ < 512) ++logged_writes_;
            LOG(Board, "[VMCU] ILP decoded tid=%u id=0x%08X value=%llu mapped=%u\n",
                tid, write.id, static_cast<unsigned long long>(write.value), target != nullptr);
        }
    }
    const auto result = unsupported ? Result::Unsupported :
        owner ? owner->apply(writes) : Result::Accepted;
    if (result == Result::Accepted) ++accepted_;
    else if (result == Result::Invalid) ++invalid_;
    else if (result == Result::Unavailable) ++unavailable_;
    else ++unsupported_;
    LOG(Board, "[VMCU] ILP completed tid=%u outcome=%u records=%zu\n", tid,
        static_cast<unsigned>(result), writes.size());
    Complete(2, tid, result == Result::Accepted);
    if (result == Result::Accepted) {
        Refresh();
        PublishPending();
    }
}

void FordSyncGen2IlpChannel::HandleInbound(const uint8_t* data, std::size_t n) {
    ++received_;
    if (n < 4) { ++malformed_; return; }
    const uint16_t tid = U16(data, 2);
    if (data[0] == 2) { HandleSet(data, n, tid); return; }
    if (data[0] != 4 && data[0] != 5 && data[0] != 6) {
        emu_.Get<Fatal>().Die("[VMCU] unmodelled ILP transaction type=%u tid=%u len=%zu", data[0], tid, n);
    }
    auto& signals = emu_.Get<FordSyncGen2IlpSignals>();
    Refresh();
    /* EA5T-14D544-BA.sec, ipc_ilprot.dll sub_C08D9A10, sub_C08D9468, sub_C08DCFEC. */
    if (data[1] != 0 || (data[0] == 5 ? n != 0x1A :
        n < 6 || n != 6u + 4u * U16(data, 4))) {
        ++malformed_; Complete(data[0], tid, false); return;
    }
    switch (data[0]) {
    case 5: {
        const uint32_t id = U32(data, 4);
        const auto sub = signals.NoteFilterRegistration(id, tid);
        /* EA5T-14D544-BA.sec, ipc_ilprot.dll sub_C08D9A10 accepts only status 0/0x40. */
        const bool retained = sub != FordSyncGen2IlpSignals::kNoSubscriber;
        if (!retained) {
            ++unavailable_;
            LOG(Caution, "[VMCU] rejected ILP filter SigID=0x%08X tid=%u: missing signal definition\n", id, tid);
        }
        Complete(5, tid, retained);
        std::vector<uint8_t> body;
        if (retained && signals.AppendSignalIndication(id, sub, body))
            Send(body.data(), body.size());
        PublishOrdered();
        break;
    }
    case 6: Complete(6, tid, true); break;
    case 4: {
        std::vector<uint8_t> body;
        signals.AppendGetAssocReply(data, n, tid, body);
        Send(body.data(), body.size());
        PublishOrdered();
        break;
    }
    default:
        emu_.Get<Fatal>().Die("[VMCU] unmodelled ILP transaction type=%u tid=%u len=%zu", data[0], tid, n);
    }
}

bool FordSyncGen2IlpChannel::QueueIndications(const std::vector<Indication>& frames) {
    if (frames.size() > kMaxIndications - indications_.size()) return false;
    auto& signals = emu_.Get<FordSyncGen2IlpSignals>();
    for (const auto& frame : frames) {
        if (frame.empty() || frame.size() != signals.CompositeSize(frame.front().id)) return false;
        std::size_t bytes = 6;
        std::vector<uint32_t> seen;
        for (const auto& field : frame) {
            const auto width = signals.HeadWriteWidth(field.id);
            uint8_t bits = 0;
            for (std::size_t i = 0; i < signals.GroundedSignalCount(); ++i)
                if (signals.GroundedSignalIdAt(i) == field.id) bits = signals.GroundedSignalBitsAt(i);
            if (!signals.SameComposite(frame.front().id, field.id) || width == 0 || width > 8 ||
                (bits < 64 && field.value >= (uint64_t{1} << bits)) ||
                std::find(seen.begin(), seen.end(), field.id) != seen.end()) return false;
            seen.push_back(field.id);
            bytes += 4 + width;
        }
        if (frame.empty() || bytes > 0x40) return false;
    }
    indications_.insert(indications_.end(), frames.begin(), frames.end());
    return true;
}

void FordSyncGen2IlpChannel::PublishOrdered() {
    auto& signals = emu_.Get<FordSyncGen2IlpSignals>();
    while (!indications_.empty()) {
        const auto& frame = indications_.front();
        if (!signals.CompositeReady(frame.front().id)) return;
        std::vector<uint32_t> ids;
        for (const auto& field : frame) {
            signals.SetReportedValue(field.id, field.value);
            ids.push_back(field.id);
        }
        std::vector<uint8_t> body;
        if (signals.AppendBatchedIndication(ids.data(), ids.size(), body, true) != frame.size())
            emu_.Get<Fatal>().Die("[VMCU] incomplete ordered ILP indication");
        Send(body.data(), body.size());
        for (const auto& field : frame) signals.MarkPublished(field.id);
        indications_.pop_front();
    }
}

void FordSyncGen2IlpChannel::PublishPending(bool cyclic) {
    PublishOrdered();
    auto& signals = emu_.Get<FordSyncGen2IlpSignals>();
    uint32_t changed[FordSyncGen2IlpSignals::kSignalCount];
    std::size_t count = signals.TakeChangedSignals(changed, std::size(changed));
    if (cyclic && count == 0) {
        const auto next = signals.NextCyclicSignal();
        if (next != FordSyncGen2IlpSignals::kTakenSignal) { changed[0] = next; count = 1; }
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto id = changed[i];
        if (signals.IsComposite(id)) continue;
        for (std::size_t sub = 0; sub < signals.SubscriberCount(id); ++sub) {
            std::vector<uint8_t> body;
            if (signals.AppendSignalIndication(id, sub, body)) Send(body.data(), body.size());
        }
        signals.MarkPublished(id);
        changed[i] = FordSyncGen2IlpSignals::kTakenSignal;
    }
    for (;;) {
        std::vector<uint8_t> body;
        std::vector<uint32_t> before(changed, changed + count);
        if (signals.AppendBatchedIndication(changed, count, body) == 0) break;
        Send(body.data(), body.size());
        for (std::size_t i = 0; i < count; ++i)
            if (before[i] != FordSyncGen2IlpSignals::kTakenSignal && changed[i] == FordSyncGen2IlpSignals::kTakenSignal)
                signals.MarkPublished(before[i]);
    }
}

void FordSyncGen2IlpChannel::OnLinkReset() {
    tx_seq_ = 0;
}

void FordSyncGen2IlpChannel::OnWatchdogPet() {
    Refresh();
    ++watchdog_pets_;
    for (const auto& device : devices_)
        if (device.watchdog) device.watchdog(watchdog_pets_);
    PublishPending(true);
}

FordSyncGen2IlpChannel::Counters FordSyncGen2IlpChannel::ReadCounters() const {
    return {accepted_.load(), unsupported_.load(), invalid_.load(), unavailable_.load(), malformed_.load(),
        received_.load(), sent_.load()};
}

void FordSyncGen2IlpChannel::RegisterDevice(Device device) {
    if (device.key.empty() || device.key.size() > 64 || devices_.size() >= 64 ||
        !device.refresh || !device.save || !device.restore || !device.reset ||
        std::any_of(devices_.begin(), devices_.end(), [&](const Device& d) { return d.key == device.key; })) {
        emu_.Get<Fatal>().Die("[VMCU] invalid ILP device registration");
    }
    devices_.push_back(std::move(device));
    std::sort(devices_.begin(), devices_.end(), [](const Device& a, const Device& b) { return a.key < b.key; });
}

void FordSyncGen2IlpChannel::Refresh(bool force) {
    for (const auto& device : devices_) device.refresh(force);
}

void FordSyncGen2IlpChannel::SaveState(StateWriter& w) const {
    w.Write("tx_seq", tx_seq_); w.Write("watchdog_pets", watchdog_pets_);
    emu_.Get<FordSyncGen2IlpSignals>().SaveState(w);
    w.Write<uint32_t>("indications_count", static_cast<uint32_t>(indications_.size()));
    for (const auto& frame : indications_) {
        w.Write<uint32_t>("fields_count", static_cast<uint32_t>(frame.size()));
        for (const auto& field : frame) {
            w.Write("signal_id", field.id); w.Write("signal_value", field.value);
        }
    }
    w.Write<uint32_t>("devices_count", static_cast<uint32_t>(devices_.size()));
    for (const auto& device : devices_) {
        w.Write<uint32_t>("key_count", static_cast<uint32_t>(device.key.size()));
        w.WriteBytes("key", device.key.data(), device.key.size());
        device.save(w);
    }
}

void FordSyncGen2IlpChannel::RestoreState(StateReader& r) {
    r.Read("tx_seq", tx_seq_); r.Read("watchdog_pets", watchdog_pets_);
    emu_.Get<FordSyncGen2IlpSignals>().RestoreState(r);
    indications_.clear();
    uint32_t frames = 0;
    r.Read("indications_count", frames);
    if (frames > kMaxIndications) r.Reject("[VMCU] too many ordered indications");
    for (uint32_t i = 0; i < frames; ++i) {
        uint32_t fields = 0;
        r.Read("fields_count", fields);
        if (fields == 0 || fields > 11) r.Reject("[VMCU] invalid indication size");
        Indication frame(fields);
        for (auto& field : frame) {
            r.Read("signal_id", field.id); r.Read("signal_value", field.value);
        }
        if (!QueueIndications({frame})) r.Reject("[VMCU] invalid ordered indication");
    }
    for (const auto& device : devices_) device.reset();
    uint32_t count = 0;
    r.Read("devices_count", count);
    if (count != devices_.size())
        r.Reject("[VMCU] the image has %u ILP devices, this build has %zu", count,
                 devices_.size());
    std::vector<std::string> seen;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t size = 0;
        r.Read("key_count", size);
        if (size == 0 || size > 64) r.Reject("[VMCU] ILP device key of %u bytes", size);
        std::string key(size, '\0');
        r.ReadBytes("key", key.data(), size);
        if (std::find(seen.begin(), seen.end(), key) != seen.end())
            r.Reject("[VMCU] ILP device '%s' appears twice", key.c_str());
        seen.push_back(key);
        const auto device = std::find_if(devices_.begin(), devices_.end(),
            [&](const auto& d) { return d.key == key; });
        if (device == devices_.end())
            r.Reject("[VMCU] ILP device '%s' is not in this build", key.c_str());
        device->restore(r);
    }
    Refresh(true);
}

}

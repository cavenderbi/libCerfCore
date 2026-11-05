#include "ford_sync_gen2_entertainment.h"
#include "ford_sync_gen2_ilp_signals.h"
#include "ford_sync_gen2_family_id.h"
#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

REGISTER_SERVICE(FordSyncGen2Entertainment);

bool FordSyncGen2Entertainment::ShouldRegister() {
    auto* board = emu_.TryGet<BoardContext>();
    return board && board->GetBoardFamilyId() == BoardFamilyId::FordSyncGen2;
}

void FordSyncGen2Entertainment::OnReady() {
    // VNIAudioSvc registers remote change filters for priority/source only.
    // Its other seven local filters still need the complete response payload.
    emu_.Get<FordSyncGen2IlpSignals>().RegisterComposite({
        0x02000092, 0x02000093, 0x02000094, 0x02000095, 0x02000096,
        0x02000097, 0x02000098, 0x02000099, 0x0200009A}, false,
        {0x02000092, 0x02000093});
    emu_.Get<Channel>().RegisterDevice({
        "entertainment",
        [this](bool force) { Refresh(force); }, {},
        [](uint32_t id) { return id >= 0x0200007E && id <= 0x02000081; },
        [this](const std::vector<Channel::Write>& writes) { return Apply(writes); },
        [this](StateWriter& w) { Save(w); },
        [this](StateReader& r) { Restore(r); },
        [this] { std::lock_guard lock(mutex_); state_ = {}; }
    });
    Refresh(true);
}

FordSyncGen2EntertainmentState FordSyncGen2Entertainment::Snapshot() const {
    std::lock_guard lock(mutex_);
    return state_;
}

FordSyncGen2IlpChannel::Result FordSyncGen2Entertainment::Apply(const std::vector<Channel::Write>& writes) {
    // VNISending!433AE050 sends priority/source/system/operation as a complete tuple.
    if (writes.size() != 4) return Channel::Result::Invalid;
    std::array<uint8_t, 4> values{};
    unsigned seen = 0;
    for (const auto& write : writes) {
        if (write.id < 0x0200007E || write.id > 0x02000081 || write.value > 255)
            return Channel::Result::Invalid;
        const auto index = write.id - 0x0200007E;
        if (seen & (1u << index)) return Channel::Result::Invalid;
        seen |= 1u << index;
        values[index] = static_cast<uint8_t>(write.value);
    }
    if (values[0] > 14 || values[1] > 15 || values[2] > 1 || values[3] > 5)
        return Channel::Result::Invalid;
    std::lock_guard lock(mutex_);
    auto next = state_;
    const auto replies = next.Apply({values[3], values[2], values[1], values[0]});
    std::vector<Channel::Indication> frames;
    for (const auto& reply : replies) {
        Channel::Indication frame;
        for (std::size_t i = 0; i < reply.size(); ++i)
            frame.push_back({0x02000092u + static_cast<uint32_t>(i), reply[i]});
        frames.push_back(std::move(frame));
    }
    if (!emu_.Get<Channel>().QueueIndications(frames)) return Channel::Result::Unavailable;
    state_ = next;
    LOG(Board, "[AHU] operation=%u source=%u priority=%u system=%u replies=%zu owner=%u\n",
        values[3], values[1], values[0], values[2], replies.size(), state_.source);
    return Channel::Result::Accepted;
}

void FordSyncGen2Entertainment::Refresh(bool force) {
    if (!force) return;
    const auto current = Snapshot().Current();
    auto& signals = emu_.Get<FordSyncGen2IlpSignals>();
    for (std::size_t i = 0; i < current.size(); ++i) {
        const auto id = 0x02000092u + static_cast<uint32_t>(i);
        signals.SetReportedValue(id, current[i]);
        // Current values answer reads / initial filters. Only queued frames are events.
        signals.MarkPublished(id);
    }
}

void FordSyncGen2Entertainment::Save(StateWriter& w) const {
    const auto state = Snapshot();
    w.Write("source", state.source); w.Write("priority", state.priority);
}

void FordSyncGen2Entertainment::Restore(StateReader& r) {
    State next;
    r.Read("source", next.source); r.Read("priority", next.priority);
    if (!(State::Supported(next.source, next.priority) || (next.source == 12 && next.priority == 14)))
        r.Reject("[AHU] invalid entertainment owner");
    std::lock_guard lock(mutex_);
    state_ = next;
}

}

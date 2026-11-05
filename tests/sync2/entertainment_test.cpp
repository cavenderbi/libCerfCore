#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/state/state_stream.h>
#include "../../src/boards/ford_sync_gen2/ford_sync_gen2_entertainment.h"
#include "../../src/boards/ford_sync_gen2/ford_sync_gen2_media_status.h"
#include "../../src/boards/ford_sync_gen2/ford_sync_gen2_ilp_signals.h"
#include "../../src/boards/ford_sync_gen2/ford_sync_gen2_vmcu_peer.h"
#include <cstdio>
#include "../../src/boards/ford_sync_gen2/ford_sync_gen2_console.h"
#include <memory>
#include <stdexcept>

extern std::vector<std::vector<uint8_t>> packets;
using namespace cerfcore;
using Channel = FordSyncGen2IlpChannel;
using State = FordSyncGen2EntertainmentState;
using cerf::le::Append16;
using cerf::le::Append32;
using cerf::le::AppendN;
using cerf::le::U16;
using cerf::le::U32;

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
class Board : public BoardContext {
public:
    using BoardContext::BoardContext;
    std::string_view GetBoardFamilyId() const override { return "ford_sync_gen2"; }
};
struct Fixture {
    CerfEmulator emu{{}, 0, nullptr};
    Fixture() {
        emu.AddCandidate<BoardContext>(std::make_unique<Board>(emu), false);
        emu.AddCandidate<FordSyncGen2VmcuPeer>(std::make_unique<FordSyncGen2VmcuPeer>(emu), false);
        emu.Bootstrap();
        packets.clear();
    }
    Channel& ChannelRef() { return emu.Get<Channel>(); }
    State Owner() { return emu.Get<FordSyncGen2Entertainment>().Snapshot(); }
    void Write(std::initializer_list<Channel::Write> fields) {
        std::vector<uint8_t> body{2, 0, 42, 0};
        Append16(body, static_cast<uint16_t>(fields.size()));
        for (const auto& field : fields) {
            Append32(body, field.id);
            AppendN(body, field.value, FordSyncGen2IlpSignals::HeadWriteWidth(field.id));
        }
        packets.clear();
        ChannelRef().HandleInbound(body.data(), body.size());
    }
    void Request(uint8_t operation, uint8_t source, uint8_t priority, uint8_t system = 0) {
        Write({{0x0200007E, priority}, {0x0200007F, source}, {0x02000080, system}, {0x02000081, operation}});
    }
    void Subscribe(unsigned count = 2) {
        auto& signals = emu.Get<FordSyncGen2IlpSignals>();
        for (unsigned i = 0; i < count; ++i) signals.NoteFilterRegistration(0x02000092 + i, 77);
    }
    std::vector<State::Batch> Events() const {
        std::vector<State::Batch> result;
        for (const auto& packet : packets) {
            if (packet[0] != 0x0A || packet.size() < 6 || U16(packet.data(), 4) != 9) continue;
            Check(packet.size() == 51, "complete nine-field frame wire size");
            State::Batch values{};
            for (unsigned i = 0; i < 9; ++i) {
                Check(U32(packet.data(), 6 + i * 5) == 0x02000092 + i, "ordered frame signal identifiers");
                values[i] = packet[10 + i * 5];
            }
            result.push_back(values);
        }
        return result;
    }
};
void FirmwareFilterMetadata() {
    Fixture f;
    auto& signals = f.emu.Get<FordSyncGen2IlpSignals>();
    Check(signals.GroundedSignalCount() == 1294, "complete firmware descriptor set");
    for (size_t i = 0; i < signals.GroundedSignalCount(); ++i) {
        const auto id = signals.GroundedSignalIdAt(i);
        const bool reported = signals.IsReported(id);
        const auto value = signals.ReportedValue(id);
        std::vector<uint8_t> filter{5, 0, 61, 0};
        Append32(filter, id); filter.resize(26);
        packets.clear(); f.ChannelRef().HandleInbound(filter.data(), filter.size());
        Check(!packets.empty() && packets[0][0] == 0x85 && packets[0][1] == 0,
            "every BCF-defined subscription is accepted");
        Check(signals.SubscriberCount(id) == 1, "known subscription is retained");
        Check(signals.IsReported(id) == reported && signals.ReportedValue(id) == value,
            "subscription does not create or alter hardware status");
        Check(FordSyncGen2IlpSignals::HeadWriteWidth(id) ==
            (signals.GroundedSignalBitsAt(i) + 7u) / 8u, "read and write descriptor widths agree");
    }
    for (auto id : {0x0000002Bu, 0x0100007Au, 0x02000469u, 0x03000000u}) {
        std::vector<uint8_t> filter{5, 0, 62, 0};
        Append32(filter, id); filter.resize(26);
        packets.clear(); f.ChannelRef().HandleInbound(filter.data(), filter.size());
        Check(packets.size() == 1 && packets[0][0] == 0x85 && packets[0][1] != 0,
            "out-of-range firmware subscription is rejected");
    }
}
void AudioListenerRegistration() {
    Fixture f;
    auto& signals = f.emu.Get<FordSyncGen2IlpSignals>();
    // VNIAudioSvc constructs these listeners before its arbitration response listener.
    // A rejected filter aborts initialization of the whole guest audio thread.
    for (auto id : {0x020000D0u, 0x020000D1u, 0x020000D3u, 0x020000D2u,
                   0x02000294u, 0x02000150u, 0x02000428u, 0x02000429u,
                   0x0200042Au, 0x0200042Bu, 0x0200042Cu}) {
        std::vector<uint8_t> filter{5, 0, 61, 0};
        Append32(filter, id); filter.resize(26);
        packets.clear(); f.ChannelRef().HandleInbound(filter.data(), filter.size());
        Check(!packets.empty() && packets[0][0] == 0x85 && packets[0][1] == 0,
            "audio startup prerequisite filter accepted");
        Check(signals.SubscriberCount(id) == 1, "audio prerequisite filter retained");
        Check(!signals.IsReported(id), "filter support does not invent DSP hardware state");
    }
    f.Subscribe(); f.Request(1, 8, 11);
    Check(f.Events().size() == 2 && f.Owner().source == 8, "audio listener can progress to USB acquire");
}
void Arbitration() {
    Fixture f;
    f.Subscribe();
    f.Request(1, 0, 7);
    auto frames = f.Events();
    Check(packets[0][0] == 0x82 && packets[0][1] == 0, "transaction completes before status events");
    Check(frames.size() == 2 && frames[0][8] == 1 && frames[1] == State::Status(0, 7, 4), "acquire acceptance then ownership");
    f.Request(1, 8, 11);
    frames = f.Events();
    Check(frames.size() == 3 && frames[0][8] == 1 && frames[1] == State::Status(0, 7, 1) &&
          frames[2] == State::Status(8, 11, 4), "handoff preserves all three ordered events");
    f.Request(1, 0, 8);
    Check(packets[0][1] == 0 && f.Events().size() == 1 && f.Events()[0][8] == 3 && f.Owner().source == 8,
          "unsupported priority denied at arbitration level without changing owner");
    f.Request(1, 0, 7, 1);
    Check(f.Events()[0][8] == 3 && f.Owner().source == 8, "rear system denied");
    f.Request(2, 0, 7);
    Check(f.Owner().source == 8, "release absent source leaves current owner");
    f.Request(4, 8, 11);
    Check(f.Events().back() == State::Status(8, 11, 4), "query matching owner");
    f.Request(5, 12, 14);
    Check(f.Events().back() == State::Status(8, 11, 4), "query all");
    f.Request(3, 12, 14);
    Check(!f.Owner().Active(), "release all");
    f.Request(2, 8, 11);
    Check(!f.Owner().Active(), "repeated release is idempotent");
    f.Request(0, 0, 7);
    Check(f.Events().empty(), "no-op has no indication");
    f.Write({{0x0200007E, 7}, {0x0200007F, 0}, {0x02000080, 0}});
    Check(packets[0][1] != 0 && !f.Owner().Active(), "incomplete tuple rejected atomically");
    f.Write({{0x0200007E, 7}, {0x0200007E, 7}, {0x02000080, 0}, {0x02000081, 1}});
    Check(packets[0][1] != 0, "duplicate field rejected");
    f.Request(6, 0, 7);
    Check(packets[0][1] != 0, "invalid operation rejected");
    packets.clear();
    f.ChannelRef().OnWatchdogPet();
    Check(f.Events().empty(), "event-only composites do not repeat cyclically");
}
void LateSubscriptionsAndRestore() {
    Fixture f;
    f.Request(1, 0, 7);
    f.Request(1, 8, 11);
    Check(f.Events().empty(), "events retained before subscriptions");
    f.Subscribe(1);
    f.ChannelRef().OnWatchdogPet();
    Check(f.Events().empty(), "no partial queued frame before final subscription");
    const std::string path = "sync2-test-state.bin";
    {
        StateWriter writer(f.emu, path);
        f.ChannelRef().SaveState(writer);
        Check(writer.Commit(), "save pending queue");
    }
    Fixture restored;
    {
        StateReader reader(restored.emu, path);
        restored.ChannelRef().RestoreState(reader);
        Check(reader.Ok(), "restore channel and devices");
    }
    Check(restored.Owner().source == 8, "owner restored");
    // Registration through the real channel must immediately release queued events.
    std::vector<uint8_t> filter{5, 0, 77, 0};
    Append32(filter, 0x02000093);
    filter.resize(26);
    restored.ChannelRef().HandleInbound(filter.data(), filter.size());
    const auto frames = restored.Events();
    Check(frames.size() == 5 && frames[0][8] == 1 && frames[1] == State::Status(0, 7, 4) &&
          frames[2][8] == 1 && frames[3] == State::Status(0, 7, 1) && frames[4] == State::Status(8, 11, 4),
          "queue survives save / restore and late subscription in exact order");
    packets.clear();
    restored.ChannelRef().OnWatchdogPet();
    Check(restored.Events().empty(), "queue drained exactly once");
    std::remove(path.c_str());
}
void Capacity() {
    Fixture f;
    bool full = false;
    for (unsigned i = 0; i < 30; ++i) {
        const auto before = f.Owner();
        f.Request(1, i % 2 ? 8 : 0, i % 2 ? 11 : 7);
        if (packets[0][1] != 0) {
            Check(f.Owner().source == before.source, "full queue leaves owner unchanged");
            full = true;
            break;
        }
    }
    Check(full, "queue has bounded capacity");
    f.Subscribe();
    f.ChannelRef().OnWatchdogPet();
    f.Request(1, 8, 11);
    Check(packets[0][1] == 0, "draining queue allows requests again");
}
void FrameValidationAndCyclicReporting() {
    Fixture f;
    Check(!f.ChannelRef().QueueIndications({{{0x02000092, 7}}}), "partial composite rejected");
    Channel::Indication frame;
    const auto current = State{}.Current();
    for (unsigned i = 0; i < 9; ++i) frame.push_back({0x02000092 + i, current[i]});
    frame[0].value = 16;
    Check(!f.ChannelRef().QueueIndications({frame}), "value beyond descriptor bits rejected");
    frame[0].value = current[0];
    frame[1].id = frame[0].id;
    Check(!f.ChannelRef().QueueIndications({frame}), "duplicate indication field rejected");
    auto& signals = f.emu.Get<FordSyncGen2IlpSignals>();
    signals.RegisterComposite({0x020001B8, 0x020001B9});
    signals.SetReportedValue(0x020001B8, 1);
    signals.SetReportedValue(0x020001B9, 0);
    signals.NoteFilterRegistration(0x020001B8, 44);
    signals.NoteFilterRegistration(0x020001B9, 44);
    signals.MarkPublished(0x020001B8);
    signals.MarkPublished(0x020001B9);
    signals.RepublishComposites();
    uint32_t changed[FordSyncGen2IlpSignals::kSignalCount];
    Check(signals.TakeChangedSignals(changed, std::size(changed)) == 2, "cyclic republish excludes entertainment events");
    Check(signals.NextCyclicSignal() == 0x020001B8, "ordinary composite retains cyclic reporting");
    {
        StateWriter writer(f.emu, "sync2-invalid-state.bin");
        writer.Write<uint8_t>("tx_seq", 0);
        writer.Write<uint32_t>("watchdog_pets", 0);
        signals.SaveState(writer);
        writer.Write<uint32_t>("indications_count", 49);
        Check(writer.Commit(), "write invalid queue snapshot");
    }
    bool rejected = false;
    try {
        StateReader reader(f.emu, "sync2-invalid-state.bin");
        f.ChannelRef().RestoreState(reader);
    } catch (const StateImageRejected&) { rejected = true; }
    Check(rejected, "invalid queue snapshot uses state rejection");
    std::remove("sync2-invalid-state.bin");
}
void Playback() {
    Fixture f;
    auto& media = f.emu.Get<FordSyncGen2MediaStatus>();
    Check(media.ReadProgress().Text() == "--:-- / --:--", "initial playback unknown");
    f.Write({{0x02000212, 100}, {0x02000213, 310}});
    Check(packets[0][1] == 0 && media.ReadProgress().Text() == "0:00 / 3:30", "offset-100 decoding");
    f.Write({{0x02000212, 165}});
    Check(media.ReadProgress().Text() == "1:05 / 3:30", "partial update preserves duration");
    f.Write({{0x02000212, 200}, {0x02000212, 210}});
    Check(packets[0][1] != 0 && media.ReadProgress().Text() == "1:05 / 3:30", "duplicate playback writes reject atomically");
    {
        StateWriter writer(f.emu, "sync2-media-state.bin");
        f.ChannelRef().SaveState(writer);
        Check(writer.Commit(), "save media status");
    }
    f.Write({{0x02000212, 0}});
    Check(media.ReadProgress().Text() == "--:-- / 3:30", "unavailable elapsed value");
    {
        StateReader reader(f.emu, "sync2-media-state.bin");
        f.ChannelRef().RestoreState(reader);
    }
    Check(media.ReadProgress().Text() == "1:05 / 3:30", "media values and valid mask restored");
    std::remove("sync2-media-state.bin");
    Check(!FordSyncGen2MediaProgress::Decode(99, true) && !FordSyncGen2MediaProgress::Decode(100, false),
          "sentinel and absent values are unknown");
}
}

void Console() {
    Fixture f;
    auto& console = f.emu.Get<FordSyncGen2Console>();
    auto& signals = f.emu.Get<FordSyncGen2IlpSignals>();
    Check(!console.Tap(255), "unknown physical button rejected");
    Check(console.Tap(50) && console.Tap(51), "queue successive track buttons");
    f.ChannelRef().OnWatchdogPet();
    Check(signals.ReportedValue(0x0200011A) == 0, "no press before filters register");
    signals.NoteFilterRegistration(0x0200011E, 88);
    f.ChannelRef().OnWatchdogPet();
    Check(signals.ReportedValue(0x0200011A) == 0, "partial filter registration cannot split button pair");
    signals.NoteFilterRegistration(0x0200011A, 88);
    std::vector<std::pair<unsigned, unsigned>> edges;
    for (unsigned i = 0; i < 4; ++i) {
        packets.clear();
        f.ChannelRef().OnWatchdogPet();
        for (const auto& packet : packets) {
            if (packet[0] != 0x0A || packet.size() != 16 || U16(packet.data(), 4) != 2) continue;
            Check(U32(packet.data(), 6) == 0x0200011E && U32(packet.data(), 11) == 0x0200011A,
                "ID and press state share one complete indication");
            edges.emplace_back(packet[10], packet[15]);
        }
    }
    Check(edges == std::vector<std::pair<unsigned,unsigned>>{{50,1},{50,0},{51,1},{51,0}},
        "rapid clicks preserve press/release order");
    for (unsigned i = 0; i < 32; ++i) Check(console.Tap(31), "bounded console queue accepts capacity");
    Check(!console.Tap(31), "console queue rejects overflow");
    console.AdjustVolume(-1000);
    Check(console.Volume() == 0, "volume lower bound");
    std::array<uint8_t, 8> pcm{0xff,0x7f,0x00,0x80,0xff,0xff,0x00,0x00};
    console.ProcessPcm16(pcm);
    Check(pcm == std::array<uint8_t,8>{}, "zero gain silences all signed PCM");
    console.AdjustVolume(1000); console.AdjustVolume(-15);
    pcm = {0xff,0x7f,0x00,0x80,0xff,0xff,0x00,0x00};
    console.ProcessPcm16(pcm);
    Check(pcm[0] == 0xff && pcm[1] == 0x3f && pcm[2] == 0 && pcm[3] == 0xc0,
        "half gain handles positive and negative full-scale without overflow");
    console.ToggleMute();
    Check(console.Muted(), "mute toggles");
    console.ProcessPcm16(pcm);
    Check(pcm == std::array<uint8_t,8>{}, "mute silences PCM");
    f.ChannelRef().OnWatchdogPet(); // Snapshot an outstanding press, before its release.
    {
        StateWriter writer(f.emu, "sync2-console-state.bin");
        f.ChannelRef().SaveState(writer);
        Check(writer.Commit(), "save console state");
    }
    Fixture restored;
    {
        StateReader reader(restored.emu, "sync2-console-state.bin");
        restored.ChannelRef().RestoreState(reader);
    }
    auto& saved = restored.emu.Get<FordSyncGen2Console>();
    Check(saved.Volume() == 15 && saved.Muted() && !saved.Tap(31), "restore amplifier and pending input queue");
    packets.clear();
    restored.ChannelRef().OnWatchdogPet();
    bool released = false;
    for (const auto& packet : packets)
        if (packet.size() == 16 && packet[0] == 0x0A && packet[10] == 31 && packet[15] == 0) released = true;
    Check(released && saved.Tap(31), "restore releases the held button and frees queue space");
    std::remove("sync2-console-state.bin");
}

int main() {
    try {
        FirmwareFilterMetadata(); AudioListenerRegistration(); Arbitration(); LateSubscriptionsAndRestore(); Capacity(); FrameValidationAndCyclicReporting(); Playback(); Console();
        std::puts("PASS arbitration, ordered transport, late subscriptions, capacity, playback, persistence");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}

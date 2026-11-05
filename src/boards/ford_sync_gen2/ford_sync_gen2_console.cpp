#include "ford_sync_gen2_console.h"
#include "ford_sync_gen2_family_id.h"
#include "ford_sync_gen2_ilp_channel.h"
#include "ford_sync_gen2_ilp_signals.h"
#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/device_buttons.h>
#include <cerfcore/host/audio_output_filter.h>
#include <cerfcore/state/state_stream.h>
#include <algorithm>
#include <array>

namespace cerfcore {
namespace {
constexpr uint32_t kButton = 0x0200011E, kPressed = 0x0200011A;
constexpr size_t kMaxTaps = 32;
struct Button { const char* id; const char* label; uint8_t code; };
// EA5T-14D544-BA VNIButtonSvc!C1523508 EFP -> EVM table;
// Media!41F464xx dispatches seek, Sound, tune, play/pause and media source.
constexpr Button kButtons[] = {
    {"audio_power", "Audio power", 31}, {"audio_source", "Media source", 79},
    {"audio_previous", "Previous track / seek down", 50},
    {"audio_next", "Next track / seek up", 51},
    {"audio_tune_down", "Tune down", 75}, {"audio_tune_up", "Tune up", 74},
    {"audio_sound", "Sound settings", 24},
    {"audio_play", "Play", 76}, {"audio_pause", "Pause", 77}
};
bool ValidButton(uint8_t code) {
    return std::any_of(std::begin(kButtons), std::end(kButtons),
        [code](const auto& button) { return button.code == code; });
}
class ConsoleButtons : public DeviceButtons {
public:
    using DeviceButtons::DeviceButtons;
    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::FordSyncGen2;
    }
    std::vector<ActionControl> Actions() override { return emu_.Get<FordSyncGen2Console>().Actions(); }
};
class ConsoleAmplifier : public AudioOutputFilter {
public:
    using AudioOutputFilter::AudioOutputFilter;
    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::FordSyncGen2;
    }
    void ProcessPcm16(std::span<uint8_t> samples) const override {
        emu_.Get<FordSyncGen2Console>().ProcessPcm16(samples);
    }
};
REGISTER_SERVICE_AS(ConsoleButtons, DeviceButtons);
REGISTER_SERVICE_AS(ConsoleAmplifier, AudioOutputFilter);
}
REGISTER_SERVICE(FordSyncGen2Console);
bool FordSyncGen2Console::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::FordSyncGen2;
}
void FordSyncGen2Console::OnReady() {
    auto& signals = emu_.Get<FordSyncGen2IlpSignals>();
    // VNI EFP listeners require ID and state with the same reception timestamp.
    signals.RegisterComposite({kButton, kPressed}, false);
    emu_.Get<FordSyncGen2IlpChannel>().RegisterDevice({
        "console", [this](bool force) {
            if (!force) return;
            std::lock_guard lock(mutex_);
            auto& s = emu_.Get<FordSyncGen2IlpSignals>();
            s.SetReportedValue(kButton, pending_.empty() ? 255 : pending_.front());
            s.SetReportedValue(kPressed, pressed_ ? 1 : 0);
            s.MarkPublished(kButton); s.MarkPublished(kPressed);
        }, [this](uint32_t) { Tick(); }, {}, {},
        [this](StateWriter& w) { Save(w); }, [this](StateReader& r) { Restore(r); },
        [this] { Reset(); }
    });
    signals.SetReportedValue(kButton, 255); signals.SetReportedValue(kPressed, 0);
    signals.MarkPublished(kButton); signals.MarkPublished(kPressed);
}
std::vector<ActionControl> FordSyncGen2Console::Actions() {
    std::vector<ActionControl> result;
    bool enabled;
    { std::lock_guard lock(mutex_); enabled = pending_.size() < kMaxTaps; }
    for (const auto& button : kButtons)
        result.push_back({button.id, button.label, enabled, [this, code = button.code] { Tap(code); }});
    result.push_back({"audio_volume_down", "Output volume down", true, [this] { AdjustVolume(-1); }});
    result.push_back({"audio_volume_up", "Output volume up", true, [this] { AdjustVolume(1); }});
    result.push_back({"audio_mute", "Mute output", true, [this] { ToggleMute(); }});
    return result;
}
bool FordSyncGen2Console::Tap(uint8_t button) {
    if (!ValidButton(button)) return false;
    std::lock_guard lock(mutex_);
    if (pending_.size() >= kMaxTaps) return false;
    pending_.push_back(button);
    return true;
}
void FordSyncGen2Console::Tick() {
    auto& signals = emu_.Get<FordSyncGen2IlpSignals>();
    if (!signals.SubscriberCount(kButton) || !signals.SubscriberCount(kPressed)) return;
    std::lock_guard lock(mutex_);
    if (pending_.empty()) return;
    // One edge per watchdog pet: the guest gets time to consume each press.
    // Retain the edge on backpressure; a release can never be dropped.
    if (!emu_.Get<FordSyncGen2IlpChannel>().QueueIndications({{{kButton, pending_.front()},
            {kPressed, pressed_ ? 0u : 1u}}})) return;
    LOG(Board, "[SYNC2 console] EFP button=%u pressed=%u\n", pending_.front(), pressed_ ? 0u : 1u);
    if (pressed_) pending_.pop_front();
    pressed_ = !pressed_;
}
void FordSyncGen2Console::AdjustVolume(int delta) {
    std::lock_guard lock(mutex_);
    volume_ = std::clamp(volume_ + std::clamp(delta, -30, 30), 0, 30);
    muted_ = false;
}
void FordSyncGen2Console::ToggleMute() { std::lock_guard lock(mutex_); muted_ = !muted_; }
int FordSyncGen2Console::Volume() const { std::lock_guard lock(mutex_); return volume_; }
bool FordSyncGen2Console::Muted() const { std::lock_guard lock(mutex_); return muted_; }
void FordSyncGen2Console::ProcessPcm16(std::span<uint8_t> samples) const {
    int gain;
    { std::lock_guard lock(mutex_); gain = muted_ ? 0 : volume_; }
    if (gain == 30) return;
    for (size_t i = 0; i + 1 < samples.size(); i += 2) {
        const auto raw = static_cast<uint16_t>(samples[i] | (uint16_t(samples[i + 1]) << 8));
        const int value = (raw < 0x8000 ? int(raw) : int(raw) - 65536) * gain / 30;
        const auto scaled = static_cast<uint16_t>(value);
        samples[i] = static_cast<uint8_t>(scaled);
        samples[i + 1] = static_cast<uint8_t>(scaled >> 8);
    }
}
void FordSyncGen2Console::Save(StateWriter& w) const {
    std::lock_guard lock(mutex_);
    w.Write("volume", volume_); w.Write("muted", muted_); w.Write("pressed", pressed_);
    w.Write<uint32_t>("taps", static_cast<uint32_t>(pending_.size()));
    for (auto button : pending_) w.Write("button", button);
}
void FordSyncGen2Console::Restore(StateReader& r) {
    int volume; bool muted, pressed; uint32_t count;
    r.Read("volume", volume); r.Read("muted", muted); r.Read("pressed", pressed); r.Read("taps", count);
    if (volume < 0 || volume > 30 || count > kMaxTaps || (pressed && !count))
        r.Reject("[SYNC2] invalid console state");
    std::deque<uint8_t> pending;
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t code; r.Read("button", code);
        if (!ValidButton(code)) r.Reject("[SYNC2] invalid console button");
        pending.push_back(code);
    }
    std::lock_guard lock(mutex_);
    volume_ = volume; muted_ = muted; pressed_ = pressed; pending_ = std::move(pending);
}
void FordSyncGen2Console::Reset() {
    std::lock_guard lock(mutex_);
    pending_.clear(); pressed_ = false; volume_ = 20; muted_ = false;
}
}

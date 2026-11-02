#include "compactflash_power_mode.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../execution/guest_cycle_clock.h"
#include <cerfcore/state/state_stream.h>

#include <algorithm>

namespace cerfcore {

int64_t CompactFlashPowerMode::NowNs() {
    return emu_.Get<GuestCycleClock>().NowNs();
}

void CompactFlashPowerMode::PowerOn() {
    SetDefaults();
    anchored_ = false;
}

void CompactFlashPowerMode::SetDefaults() {
    sleeping_      = false;
    timer_enabled_ = true;
    timer_ns_      = kTimerStepNs;
}

void CompactFlashPowerMode::HostAccess() {
    if (anchored_) return;
    idle_since_ns_ = NowNs();
    anchored_      = true;
}

bool CompactFlashPowerMode::Sleeping() {
    if (sleeping_) return true;
    if (!timer_enabled_ || !anchored_) return false;
    return NowNs() - idle_since_ns_ >= timer_ns_;
}

void CompactFlashPowerMode::SetIdleTimer(uint8_t count) {
    timer_enabled_ = count != 0u;
    if (timer_enabled_) timer_ns_ = kTimerStepNs * count;
}

void CompactFlashPowerMode::MarkActivity() {
    idle_since_ns_ = NowNs();
    anchored_      = true;
}

void CompactFlashPowerMode::SaveState(StateWriter& w) {
    const int64_t idle_ns = anchored_ ? std::max<int64_t>(NowNs() - idle_since_ns_, 0) : 0;
    w.Write<uint8_t>("sleeping", sleeping_ ? 1u : 0u);
    w.Write<uint8_t>("idle_timer_enabled", timer_enabled_ ? 1u : 0u);
    w.Write("idle_timer_ns", static_cast<uint64_t>(timer_ns_));
    w.Write<uint8_t>("idle_anchored", anchored_ ? 1u : 0u);
    w.Write("idle_elapsed_ns", static_cast<uint64_t>(idle_ns));
}

void CompactFlashPowerMode::RestoreState(StateReader& r) {
    uint8_t  sleeping = 0;
    uint8_t  enabled  = 0;
    uint8_t  anchored = 0;
    uint64_t timer_ns = 0;
    uint64_t idle_ns  = 0;
    r.Read("sleeping", sleeping);
    r.Read("idle_timer_enabled", enabled);
    r.Read("idle_timer_ns", timer_ns);
    r.Read("idle_anchored", anchored);
    r.Read("idle_elapsed_ns", idle_ns);
    sleeping_      = sleeping != 0u;
    timer_enabled_ = enabled != 0u;
    timer_ns_      = static_cast<int64_t>(timer_ns);
    anchored_      = anchored != 0u;
    idle_since_ns_ = anchored_ ? NowNs() - static_cast<int64_t>(idle_ns) : 0;
}

}

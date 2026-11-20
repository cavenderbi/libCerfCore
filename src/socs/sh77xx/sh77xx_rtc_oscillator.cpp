#include "sh77xx_rtc_oscillator.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfcore/state/state_stream.h>

#include <utility>

namespace cerfcore {

namespace {

constexpr uint64_t kRtcclkDivider = 2u;

}

uint64_t Sh77xxRtcOscillator::RtcclkTicks() { return osc_.Now() / kRtcclkDivider; }

void Sh77xxRtcOscillator::ArmAtRtcclk(GuestCycleClock::Event* event, uint64_t tick) {
    osc_.ArmAt(event, tick * kRtcclkDivider);
}

int64_t Sh77xxRtcOscillator::RtcclkSleptNsAtTick(uint64_t tick) { return CrystalSleptNsAtTick(tick * kRtcclkDivider); }

void Sh77xxRtcOscillator::RegisterRtcclkListener(std::function<void()> listener) {
    rtcclk_listeners_.Add(std::move(listener));
}

void Sh77xxRtcOscillator::AttachOscillator() {
    osc_.Attach(kCrystalHz, 1u);
    emu_.Get<GuestCycleClock>().RegisterRateListener([this] {
        osc_.Rescale();
        OnOscillatorRateChange();
        NotifyRtcclk();
    });
}

void Sh77xxRtcOscillator::SetOscillatorRunning(bool running) {
    running_ = running;
    osc_.SetCounting(running);
    NotifyRtcclk();
}

void Sh77xxRtcOscillator::RestoreOscillatorRunning(bool running) { running_ = running; }

void Sh77xxRtcOscillator::NotifyRtcclk() { rtcclk_listeners_.Call(); }

uint64_t Sh77xxRtcOscillator::CrystalTicks() { return osc_.Now(); }

void Sh77xxRtcOscillator::ArmAtCrystalTick(GuestCycleClock::Event* event, uint64_t tick) { osc_.ArmAt(event, tick); }

int64_t Sh77xxRtcOscillator::CrystalSleptNsAtTick(uint64_t tick) {
    if (!running_) return GuestDeepSleep::kNoParkWake;
    return osc_.SleptNsAtTick(tick);
}

void Sh77xxRtcOscillator::SaveOscillator(StateWriter& w) { osc_.Save(w); }

void Sh77xxRtcOscillator::RestoreOscillator(StateReader& r) { osc_.Restore(r); }

void Sh77xxRtcOscillator::AttachDivider(uint64_t period) {
    divider_period_ = period;
    seen_           = CrystalTicks();
    clock_          = &emu_.Get<GuestCycleClock>();
    divider_event_  = clock_->Add([this] { OnDividerEvent(); });
    GuestDeepSleep& sleep = emu_.Get<GuestDeepSleep>();
    sleep.RegisterParkClock([this] { OnDividerEvent(); });
    sleep.RegisterParkWakeDue([this] { return ParkWakeDueNs(); });
}

Sh77xxRtcOscillator::DividerStep Sh77xxRtcOscillator::StepDivider() {
    const uint64_t    now = CrystalTicks();
    const DividerStep step{phase_, phase_ + (now - seen_)};
    seen_  = now;
    phase_ = static_cast<uint32_t>(step.to % divider_period_);
    return step;
}

uint64_t Sh77xxRtcOscillator::NextBoundaryTick(uint64_t period) const { return seen_ + period - phase_ % period; }

uint64_t Sh77xxRtcOscillator::SecondBoundaryTick(uint64_t seconds) const {
    return NextBoundaryTick(kCrystalHz) + (seconds - 1u) * kCrystalHz;
}

void Sh77xxRtcOscillator::ScheduleTick(uint64_t tick) {
    if (tick == kNoTick) {
        clock_->Disarm(divider_event_);
        return;
    }
    ArmAtCrystalTick(divider_event_, tick);
}

void Sh77xxRtcOscillator::SaveDivider(StateWriter& w) { w.Write("phase", phase_); }

void Sh77xxRtcOscillator::RestoreDivider(StateReader& r) {
    r.Read("phase", phase_);
    seen_ = CrystalTicks();
}

int64_t Sh77xxRtcOscillator::ParkWakeDueNs() {
    OnDividerEvent();
    const uint64_t next = NextWakingTick();
    if (next == kNoTick) return GuestDeepSleep::kNoParkWake;
    return CrystalSleptNsAtTick(next);
}

}

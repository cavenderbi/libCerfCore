#include "sh77xx_timer_clock.h"

#include <cerfcore/core/cerf_emulator.h>
#include "sh77xx_rtc_oscillator.h"

#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kPclkTapShift = 2u;

}

void Sh77xxTimerClock::Attach(std::function<void()> on_pclk_retime, std::function<void()> on_rtcclk_change) {
    clock_ = &emu_.Get<GuestCycleClock>();
    rtc_   = &emu_.Get<Sh77xxRtcOscillator>();
    pclk_.Attach(std::move(on_pclk_retime));
    rtc_->RegisterRtcclkListener(std::move(on_rtcclk_change));
}

uint32_t Sh77xxTimerClock::PclkShift(uint16_t tpsc) { return kPclkTapShift + 2u * tpsc; }

uint64_t Sh77xxTimerClock::Ticks(uint16_t tpsc) {
    if (Rtcclk(tpsc)) return rtc_->RtcclkTicks();
    return pclk_.DividedTicks(PclkShift(tpsc));
}

void Sh77xxTimerClock::ArmAt(GuestCycleClock::Event* event, uint64_t tick, uint16_t tpsc) {
    if (Rtcclk(tpsc)) {
        rtc_->ArmAtRtcclk(event, tick);
        return;
    }
    clock_->Arm(event, pclk_.CycleOfDividedTick(tick, PclkShift(tpsc)));
}

int64_t Sh77xxTimerClock::RtcclkSleptNsAtTick(uint64_t tick) { return rtc_->RtcclkSleptNsAtTick(tick); }

void Sh77xxTimerClock::Save(StateWriter& w) const { pclk_.Save(w); }

void Sh77xxTimerClock::Restore(StateReader& r) { pclk_.Restore(r); }

void Sh77xxTimerClock::PostRestore() { pclk_.PostRestore(); }

}

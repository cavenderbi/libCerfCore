#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../../execution/guest_cycle_clock.h"
#include "../oscillator_ticks.h"

#include <cstdint>
#include <functional>
#include <limits>

namespace cerfcore {

class Sh77xxRtcOscillator : public Peripheral {
public:
    static constexpr uint64_t kCrystalHz = 32768u;
    static constexpr uint64_t kNoTick    = std::numeric_limits<uint64_t>::max();

    using Peripheral::Peripheral;

    uint64_t RtcclkTicks();
    void     ArmAtRtcclk(GuestCycleClock::Event* event, uint64_t tick);
    int64_t  RtcclkSleptNsAtTick(uint64_t tick);
    void     RegisterRtcclkListener(std::function<void()> listener);
    bool     OscillatorRunning() const { return running_; }

protected:
    struct DividerStep {
        uint64_t from;
        uint64_t to;

        uint64_t Seconds() const { return to / kCrystalHz - from / kCrystalHz; }
    };

    virtual void     OnOscillatorRateChange() {}
    virtual void     OnDividerEvent() = 0;
    virtual uint64_t NextWakingTick() = 0;

    void     AttachOscillator();
    void     SetOscillatorRunning(bool running);
    void     RestoreOscillatorRunning(bool running);
    void     NotifyRtcclk();
    uint64_t CrystalTicks();
    void     ArmAtCrystalTick(GuestCycleClock::Event* event, uint64_t tick);
    int64_t  CrystalSleptNsAtTick(uint64_t tick);
    void     SaveOscillator(StateWriter& w);
    void     RestoreOscillator(StateReader& r);

    void        AttachDivider(uint64_t period);
    DividerStep StepDivider();
    void        RestartDivider() { phase_ = 0u; }
    uint32_t    DividerPhase() const { return phase_; }
    uint64_t    NextBoundaryTick(uint64_t period) const;
    uint64_t    SecondBoundaryTick(uint64_t seconds) const;
    void        ScheduleTick(uint64_t tick);
    void        SaveDivider(StateWriter& w);
    void        RestoreDivider(StateReader& r);

private:
    int64_t ParkWakeDueNs();

    StoppableOscillatorTicks osc_{emu_, true};
    CallbackList<>           rtcclk_listeners_;
    GuestCycleClock*         clock_          = nullptr;
    GuestCycleClock::Event*  divider_event_  = nullptr;
    uint64_t                 divider_period_ = kCrystalHz;
    uint64_t                 seen_           = 0u;
    uint32_t                 phase_          = 0u;
    bool                     running_        = true;
};

}

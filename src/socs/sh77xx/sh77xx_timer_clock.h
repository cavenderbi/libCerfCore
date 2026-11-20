#pragma once

#include "../../execution/guest_cycle_clock.h"
#include "sh77xx_peripheral_clock_ticks.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

class CerfEmulator;
class Sh77xxRtcOscillator;
class StateReader;
class StateWriter;

class Sh77xxTimerClock {
public:
    static constexpr uint16_t kTpscPclkLast = 3u;
    static constexpr uint16_t kTpscRtcclk   = 4u;

    explicit Sh77xxTimerClock(CerfEmulator& emu) : emu_(emu), pclk_(emu) {}

    void Attach(std::function<void()> on_pclk_retime, std::function<void()> on_rtcclk_change);

    static bool Rtcclk(uint16_t tpsc) { return tpsc == kTpscRtcclk; }
    static bool HasSource(uint16_t tpsc) { return tpsc <= kTpscPclkLast || Rtcclk(tpsc); }

    uint64_t Ticks(uint16_t tpsc);
    void     ArmAt(GuestCycleClock::Event* event, uint64_t tick, uint16_t tpsc);
    int64_t  RtcclkSleptNsAtTick(uint64_t tick);

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);
    void PostRestore();

private:
    static uint32_t PclkShift(uint16_t tpsc);

    CerfEmulator&              emu_;
    GuestCycleClock*           clock_ = nullptr;
    Sh77xxRtcOscillator*       rtc_   = nullptr;
    Sh77xxPeripheralClockTicks pclk_;
};

}

#pragma once

#include "../../execution/guest_cycle_clock.h"
#include "../cycle_anchored_counter.h"
#include "../rated_tick_count.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;
class Sh77xxCpg;
class Sh77xxFrqcr;

enum class Sh77xxTickedClock : uint8_t {
    kPeripheral,
    kCkio,
};

class Sh77xxClockTicks {
public:
    Sh77xxClockTicks(CerfEmulator& emu, Sh77xxTickedClock clock) : emu_(emu), clock_kind_(clock), ticks_(emu) {}

    void     Attach(std::function<void()> on_retime);
    uint64_t DividedTicks(uint32_t shift) const;
    uint64_t CycleOfDividedTick(uint64_t tick, uint32_t shift) const;

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);
    void PostRestore();

private:
    GuestCycleClock::Rate SourceRate(uint16_t frqcr) const;
    uint64_t              Ticks() const;
    uint64_t              CycleOfTick(uint64_t tick) const;
    void                  Retime();

    CerfEmulator&           emu_;
    const Sh77xxTickedClock clock_kind_;
    GuestCycleClock*        clock_ = nullptr;
    Sh77xxFrqcr*            frqcr_ = nullptr;
    Sh77xxCpg*              cpg_   = nullptr;
    RatedTickCount          ticks_;
    GuestCycleClock::Rate   rate_{};
    std::function<void()>   on_retime_;
    uint64_t                restored_ticks_ = 0u;
    CounterPhase            restored_phase_{};
};

}

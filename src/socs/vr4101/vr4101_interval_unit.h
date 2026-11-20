#pragma once

#include "../../execution/guest_cycle_clock.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint64_t kVr4101TclocksPerIntervalUnit = 500u;

constexpr GuestCycleClock::Rate Vr4101IntervalUnitRate(GuestCycleClock::Rate pclock, uint64_t cycles_per_tclk) {
    return GuestCycleClock::Rate{pclock.num, pclock.den * kVr4101TclocksPerIntervalUnit * cycles_per_tclk};
}

}

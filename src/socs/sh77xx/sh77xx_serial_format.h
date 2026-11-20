#pragma once

#include "../../execution/guest_cycle_clock.h"
#include "../../peripherals/serial/serial_line.h"

#include <cstdint>

namespace cerfcore {

uint64_t               Sh77xxSerialBitPclkTicks(uint8_t smr, uint8_t brr);
GuestCycleClock::Rate  Sh77xxSerialBitRate(uint8_t smr, uint8_t brr, GuestCycleClock::Rate pclk);
uint32_t               Sh77xxSerialBaud(uint8_t smr, uint8_t brr, GuestCycleClock::Rate pclk);
SerialLine::LineConfig Sh77xxSerialFrameFormat(uint8_t smr);

}

#include "sh77xx_port_data.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>

namespace cerfcore {

Sh77xxIntcPin Sh77xxPortData::IntcPinOf(uint32_t port, uint32_t pin) const {
    emu_.Get<Fatal>().Die("Sh77xxPortData: port %u pin %u carries no interrupt controller input", port, pin);
}

void Sh77xxPortData::SetSerialEnables(uint32_t port, uint32_t pin, bool transmit, bool receive) {
    const uint8_t bit    = static_cast<uint8_t>(1u << pin);
    const uint8_t before = transmitting_[port];
    transmitting_[port]  = static_cast<uint8_t>(transmit ? before | bit : before & ~bit);
    receiving_[port]     = static_cast<uint8_t>(receive ? receiving_[port] | bit : receiving_[port] & ~bit);
    if (transmitting_[port] != before) function_listeners_.Call();
}

void Sh77xxPortData::SetFunctionOutputs(uint32_t port, uint8_t pins, PinDrive drive) {
    const uint8_t low_before  = function_low_[port];
    const uint8_t high_before = function_high_[port];
    function_low_[port]  = static_cast<uint8_t>(drive == PinDrive::kLow ? low_before | pins : low_before & ~pins);
    function_high_[port] = static_cast<uint8_t>(drive == PinDrive::kHigh ? high_before | pins : high_before & ~pins);
    if (function_low_[port] != low_before || function_high_[port] != high_before) function_listeners_.Call();
}

}

#include "casio_cassiopeia_a10_led_control.h"

#include "casio_cassiopeia_a10_members.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10LedControl);

namespace {

constexpr uint16_t kControlBits = 0x000Fu;
constexpr uint16_t kBlinkEnable = 0x0002u;
constexpr uint16_t kSteadyOn    = 0x0005u;

}

bool CasioCassiopeiaA10LedControl::ShouldRegister() {
    return CasioCassiopeiaA10StoredChipRegister::ShouldRegister() &&
           CasioCassiopeiaA10IsA51Class(emu_.Get<BoardContext>().GetBoardId());
}

bool CasioCassiopeiaA10LedControl::Blinking() const { return (Value() & kBlinkEnable) != 0u; }

bool CasioCassiopeiaA10LedControl::Steady() const { return (Value() & kSteadyOn) == kSteadyOn; }

bool CasioCassiopeiaA10LedControl::Accepts(uint16_t value) const {
    if ((value & ~kControlBits) != 0u) return false;
    const uint16_t steady = value & kSteadyOn;
    if (steady == 0u) return true;
    return steady == kSteadyOn && (value & kBlinkEnable) == 0u;
}

}

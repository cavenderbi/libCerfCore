#pragma once

#include <cstdint>

namespace cerfcore {

namespace sh77xx_intc {

struct AbsentInputs {
    uint32_t input_pins = 0u;
    uint32_t irl_pins   = 0u;

    uint32_t IrlInUse(bool irl_mode, bool irls_in_use) const;
    uint32_t IrqPins() const;
    uint32_t Pints() const;
    uint32_t Observed(uint32_t irqs, uint32_t pints, bool irl_mode, bool irls_in_use) const;
};

}

}

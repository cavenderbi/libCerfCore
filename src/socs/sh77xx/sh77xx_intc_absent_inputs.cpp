#include "sh77xx_intc_absent_inputs.h"

#include "sh77xx_intc_model.h"

namespace cerfcore {

namespace sh77xx_intc {

uint32_t AbsentInputs::IrlInUse(bool irl_mode, bool irls_in_use) const {
    if (!irl_mode) return 0u;
    return irl_pins | (irls_in_use ? input_pins & kIrlsPins : 0u);
}

uint32_t AbsentInputs::IrqPins() const { return input_pins & ((1u << kIrqPins) - 1u); }

uint32_t AbsentInputs::Pints() const { return input_pins >> kPintBit0; }

uint32_t AbsentInputs::Observed(uint32_t irqs, uint32_t pints, bool irl_mode, bool irls_in_use) const {
    return (input_pins & irqs) | ((pints & Pints()) << kPintBit0) | IrlInUse(irl_mode, irls_in_use);
}

}

}

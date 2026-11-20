#include "sh77xx_intc_model.h"

namespace cerfcore {

namespace sh77xx_intc {

uint32_t Model::SelectableIrqPins(Scope scope, uint32_t irl_pins) const {
    const uint32_t count = !irq_pins ? 0u : scope == Scope::kStandbyWake ? standby_wake_irq_pins : kIrqPins;
    return ((1u << count) - 1u) & ~irl_pins;
}

bool Model::SelectsPint(Scope scope) const { return scope != Scope::kStandbyWake || standby_wake_pint; }

uint16_t Model::IprfHaltBits() const { return lcdc_pcc ? kIprfReserved : 0xFFFFu; }

}

}

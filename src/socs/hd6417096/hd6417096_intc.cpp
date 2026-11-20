#include "../sh77xx/sh77xx_intc_impl.h"
#include "hd6417096_id.h"

namespace cerfcore {

namespace {

constexpr sh77xx_intc::Model kHd6417096Intc{.irls                  = false,
                                            .lcdc_pcc              = false,
                                            .standby_wake_irq_pins = 0u,
                                            .standby_wake_pint     = false,
                                            .area1_registers       = false,
                                            .area7_extra_ipr       = true,
                                            .irq_pins              = false};

class Hd6417096Intc : public sh77xx_intc::Sh77xxIntcBase<SocId::Hd6417096, kHd6417096Intc> {
public:
    using Sh77xxIntcBase::Sh77xxIntcBase;
};

}

REGISTER_SERVICE_AS(Hd6417096Intc, Sh77xxIntc);

}

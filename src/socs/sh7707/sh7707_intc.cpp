#include "../sh77xx/sh77xx_intc_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr sh77xx_intc::Model kSh7707Intc{
    .irls = false, .lcdc_pcc = true, .standby_wake_irq_pins = 4u, .standby_wake_pint = false};

class Sh7707Intc : public sh77xx_intc::Sh77xxIntcBase<SocId::Sh7707, kSh7707Intc> {
public:
    using Sh77xxIntcBase::Sh77xxIntcBase;
};

}

REGISTER_SERVICE_AS(Sh7707Intc, Sh77xxIntc);

}

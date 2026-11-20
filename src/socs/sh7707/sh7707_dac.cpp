#include "../sh77xx/sh77xx_dac_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kSh7707Dac =
    sh77xx_dac::WindowOf(cerf_reg_window_detail::OtherReset::kReset);

constexpr uint8_t kSh7707Stbcr2Mstp9 = 0x40u;

class Sh7707Dac : public sh77xx_dac::Sh77xxDacBase<SocId::Sh7707, kSh7707Dac, kSh7707Stbcr2Mstp9> {
public:
    using Sh77xxDacBase::Sh77xxDacBase;
};

}

REGISTER_SERVICE_AS(Sh7707Dac, Sh77xxDac);

}

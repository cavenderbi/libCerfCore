#include "../sh77xx/sh77xx_power_down_impl.h"
#include "../sh77xx/sh77xx_standby_control.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kSh7707Stbcr = sh77xx_power_down::Stbcr(
    0x00u, cerf_reg_window_detail::OtherReset::kRetain);

class Sh7707Stbcr : public sh77xx_power_down::StandbyControlRegister<SocId::Sh7707, kSh7707Stbcr> {
public:
    using StandbyControlRegister::StandbyControlRegister;
};

}

REGISTER_SERVICE_AS(Sh7707Stbcr, Sh77xxStbcr);

}

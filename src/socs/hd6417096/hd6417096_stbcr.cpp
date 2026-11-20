#include "../sh77xx/sh77xx_power_down_impl.h"
#include "../sh77xx/sh77xx_standby_control.h"
#include "hd6417096_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kHd6417096Stbcr = sh77xx_power_down::Stbcr(
    0x78u, cerf_reg_window_detail::OtherReset::kRetain);

class Hd6417096Stbcr : public sh77xx_power_down::StandbyControlRegister<SocId::Hd6417096, kHd6417096Stbcr, true> {
public:
    using StandbyControlRegister::StandbyControlRegister;
};

}

REGISTER_SERVICE_AS(Hd6417096Stbcr, Sh77xxStbcr);

}

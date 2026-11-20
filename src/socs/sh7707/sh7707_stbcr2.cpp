#include "../sh77xx/sh77xx_power_down_impl.h"
#include "../sh77xx/sh77xx_standby_control.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr uint8_t kSh7707Stbcr2ModuleStops = 0xFFu;

constexpr cerf_reg_window_detail::RegWindowModel kSh7707Stbcr2 =
    sh77xx_power_down::Stbcr2(kSh7707Stbcr2ModuleStops, 0x00u, cerf_reg_window_detail::OtherReset::kRetain);

class Sh7707Stbcr2
    : public sh77xx_power_down::ModuleStopRegister<SocId::Sh7707, kSh7707Stbcr2, Sh77xxStbcr2,
                                                   kSh7707Stbcr2ModuleStops> {
public:
    using ModuleStopRegister::ModuleStopRegister;
};

}

REGISTER_SERVICE_AS(Sh7707Stbcr2, Sh77xxStbcr2);

}

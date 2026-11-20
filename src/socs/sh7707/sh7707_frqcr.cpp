#include "../sh77xx/sh77xx_frqcr_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kSh7707Frqcr = sh77xx_frqcr::Window(0x03FFu, 0xFE00u, false);

class Sh7707Frqcr : public sh77xx_frqcr::Sh77xxFrqcrBase<SocId::Sh7707, kSh7707Frqcr> {
public:
    using Sh77xxFrqcrBase::Sh77xxFrqcrBase;
};

}

REGISTER_SERVICE_AS(Sh7707Frqcr, Sh77xxFrqcr);

}

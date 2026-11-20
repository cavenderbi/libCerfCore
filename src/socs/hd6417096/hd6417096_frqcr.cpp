#include "../sh77xx/sh77xx_frqcr_impl.h"
#include "hd6417096_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kHd6417096Frqcr = sh77xx_frqcr::Window(0x01FFu, 0xFE00u, false);

class Hd6417096Frqcr : public sh77xx_frqcr::Sh77xxFrqcrBase<SocId::Hd6417096, kHd6417096Frqcr> {
public:
    using Sh77xxFrqcrBase::Sh77xxFrqcrBase;
};

}

REGISTER_SERVICE_AS(Hd6417096Frqcr, Sh77xxFrqcr);

}

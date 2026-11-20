#include "../sh77xx/sh77xx_frqcr_impl.h"
#include "sh7729_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kSh7729Frqcr = sh77xx_frqcr::Window(0xE3FFu, 0x1E00u, true);

class Sh7729Frqcr : public sh77xx_frqcr::Sh77xxFrqcrBase<SocId::Sh7729, kSh7729Frqcr> {
public:
    using Sh77xxFrqcrBase::Sh77xxFrqcrBase;
};

}

REGISTER_SERVICE_AS(Sh7729Frqcr, Sh77xxFrqcr);

}

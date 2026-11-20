#include "../sh77xx/sh77xx_ccr_impl.h"
#include "sh7729_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kSh7729Ccr =
    sh77xx_ccr::Window(0x0007u, 0xFFF0u, 0xFFFFu, cerf_reg_window_detail::OtherReset::kReset);

class Sh7729Ccr : public cerf_reg_window_detail::RegWindowBase<SocId::Sh7729, kSh7729Ccr> {
public:
    using RegWindowBase::RegWindowBase;
};

}

REGISTER_SERVICE(Sh7729Ccr);

}

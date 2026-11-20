#include "../sh77xx/sh77xx_ccr_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kSh7707Ccr =
    sh77xx_ccr::Window(0x0027u, 0x0030u, 0x0000u, cerf_reg_window_detail::OtherReset::kReset);

class Sh7707Ccr : public cerf_reg_window_detail::RegWindowBase<SocId::Sh7707, kSh7707Ccr> {
public:
    using RegWindowBase::RegWindowBase;
};

}

REGISTER_SERVICE(Sh7707Ccr);

}

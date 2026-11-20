#include "../sh77xx/sh77xx_ccr_impl.h"
#include "hd6417096_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kHd6417096Ccr =
    sh77xx_ccr::Window(0x0063u, 0xFFB4u, 0xFFF6u, cerf_reg_window_detail::OtherReset::kReset);

class Hd6417096Ccr : public cerf_reg_window_detail::RegWindowBase<SocId::Hd6417096, kHd6417096Ccr> {
public:
    using RegWindowBase::RegWindowBase;
};

}

REGISTER_SERVICE(Hd6417096Ccr);

}

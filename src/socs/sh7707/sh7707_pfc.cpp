#include "../sh77xx/sh77xx_pfc_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kSh7707ControlReset[kSh77xxPortCount] = {
    0x0000u, 0x0000u, 0xAAAAu, 0xAAAAu, 0xAAAAu, 0xAAAAu, 0xAAAAu, 0xAAAAu, 0x0000u, 0x0000u, 0x0000u, 0xA888u,
};

constexpr cerf_reg_window_detail::RegWindowModel kSh7707PfcControl =
    sh77xx_pfc::ControlWindow(kSh7707ControlReset, 0u);

class Sh7707Pfc : public sh77xx_pfc::Sh77xxPfcBase<SocId::Sh7707, kSh7707PfcControl> {
public:
    using Sh77xxPfcBase::Sh77xxPfcBase;
};

}

REGISTER_SERVICE_AS(Sh7707Pfc, Sh77xxPfc);

}

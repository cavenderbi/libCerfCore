#include "../vr41xx/vr41xx_hsp_impl.h"
#include "vr4111_id.h"

namespace cerfcore {

namespace {

constexpr cerf_vr41xx_hsp_detail::Vr41xxHspChip kVr4111HspChip{0xA5u, false};

class Vr4111Hsp : public cerf_vr41xx_hsp_detail::Vr41xxHspBase<SocId::Vr4111, kVr4111HspChip> {
public:
    using Vr41xxHspBase::Vr41xxHspBase;
};

}

REGISTER_SERVICE_AS(Vr4111Hsp, Vr41xxHsp);

}

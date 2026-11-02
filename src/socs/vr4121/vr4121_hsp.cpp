#include "../vr41xx/vr41xx_hsp_impl.h"
#include "vr4121_id.h"

namespace cerfcore {

namespace {

constexpr cerf_vr41xx_hsp_detail::Vr41xxHspChip kVr4121HspChip{0x5Au, false};

class Vr4121Hsp : public cerf_vr41xx_hsp_detail::Vr41xxHspBase<SocId::Vr4121, kVr4121HspChip> {
public:
    using Vr41xxHspBase::Vr41xxHspBase;
};

}

REGISTER_SERVICE(Vr4121Hsp);

}

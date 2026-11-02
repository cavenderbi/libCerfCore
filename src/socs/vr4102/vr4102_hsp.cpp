#include "../vr41xx/vr41xx_hsp_impl.h"
#include "vr4102_id.h"

namespace cerfcore {

namespace {

constexpr cerf_vr41xx_hsp_detail::Vr41xxHspChip kVr4102HspChip{0xA5u, true};

class Vr4102Hsp : public cerf_vr41xx_hsp_detail::Vr41xxHspBase<SocId::Vr4102, kVr4102HspChip> {
public:
    using Vr41xxHspBase::Vr41xxHspBase;
};

}

REGISTER_SERVICE(Vr4102Hsp);

}

#include "../vr41xx/vr41xx_icu_impl.h"
#include "vr4101_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

using cerf_vr41xx_icu_detail::Vr41xxIcuBase;
using cerf_vr41xx_icu_detail::Vr41xxIcuModel;

constexpr Vr41xxIcuModel kModel = {
    0x0B000080u,
    0x20u,
    0u,
    0u,
    0x341Fu,
    0x0000u,
    0x0000u,
    0x3FFFu,
    0x001Fu,
    0x000Fu,
    0x001Fu,
    0x2FFFu,
    0x3FFFu,
    0x0000u,
    0x0000u,
    0x0000u,
    0x0200u,
    0x0001u,
};

class Vr4101Icu : public Vr41xxIcuBase<SocId::Vr4101, kModel> {
public:
    using Vr41xxIcuBase::Vr41xxIcuBase;
};

}

REGISTER_SERVICE_AS(Vr4101Icu, Vr41xxIcu);

}

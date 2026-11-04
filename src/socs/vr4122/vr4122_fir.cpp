#include "../vr41xx/vr41xx_reg_window_impl.h"
#include "vr4122_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

using cerf_vr41xx_reg_window_detail::ReadKind;
using cerf_vr41xx_reg_window_detail::Vr41xxRegWindowBase;
using cerf_vr41xx_reg_window_detail::Vr41xxRegWindowModel;
using cerf_vr41xx_reg_window_detail::WriteKind;

constexpr uint32_t kOffFrstr = 0x00u;
constexpr uint32_t kOffIrsr1 = 0x18u;

constexpr Vr41xxRegWindowModel Vr4122FirModel() {
    Vr41xxRegWindowModel m{0x0F000840u, kOffIrsr1 + 2u, kOffIrsr1 / 2u + 1u, false, {}};
    m.reg[kOffFrstr / 2u] = {ReadKind::kZero, WriteKind::kFatal};
    m.reg[kOffIrsr1 / 2u] = {ReadKind::kZero, WriteKind::kDrop};
    return m;
}

constexpr Vr41xxRegWindowModel kVr4122FirModel = Vr4122FirModel();

class Vr4122Fir : public Vr41xxRegWindowBase<SocId::Vr4122, kVr4122FirModel> {
public:
    using Vr41xxRegWindowBase::Vr41xxRegWindowBase;

protected:
    const char* RegStateName(uint32_t i) const override {
        (void)i;
        return "fir_reg";
    }
};

}

REGISTER_SERVICE(Vr4122Fir);

}

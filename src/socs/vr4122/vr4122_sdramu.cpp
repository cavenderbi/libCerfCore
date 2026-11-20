#include "../reg_window_impl.h"
#include "vr4122_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegWindowBase;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

constexpr uint32_t kOffBcuRfCnt  = 0x04u;
constexpr uint32_t kOffRamSize   = 0x08u;

constexpr RegWindowModel Vr4122SdramuModel() {
    RegWindowModel m{0x0F000400u, kOffRamSize + 2u, kOffRamSize / 2u + 1u, false, {}};
    m.reg[kOffBcuRfCnt / 2u] = {ReadKind::kStored, WriteKind::kStored, 0x3FFFu, 0x0200u, 0xC000u};
    return m;
}

constexpr RegWindowModel kVr4122SdramuModel = Vr4122SdramuModel();

class Vr4122Sdramu : public RegWindowBase<SocId::Vr4122, kVr4122SdramuModel> {
public:
    using RegWindowBase::RegWindowBase;

protected:
    const char* RegStateName(uint32_t i) const override {
        (void)i;
        return "sdramu_reg";
    }
};

}

REGISTER_SERVICE(Vr4122Sdramu);

}

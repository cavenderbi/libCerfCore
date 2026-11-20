#include "../vr41xx/vr41xx_cmu_window.h"
#include "../reg_window_impl.h"
#include "vr4101_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

constexpr RegWindowModel kModel = {
    0x0B000060u,
    0x20u,
    1u,
    false,
    {
        { ReadKind::kStored, WriteKind::kStored, 0x007Fu, 0x0000u, 0xFF80u },
    },
};

class Vr4101Cmu : public Vr41xxCmuWindow<SocId::Vr4101, kModel> {
public:
    using Vr41xxCmuWindow::Vr41xxCmuWindow;
};

}

REGISTER_SERVICE_AS(Vr4101Cmu, Vr41xxCmu);

}

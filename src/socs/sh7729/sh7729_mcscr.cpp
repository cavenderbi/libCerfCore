#include "../reg_window_impl.h"
#include "sh7729_id.h"

namespace cerfcore {

namespace {

using cerf_reg_window_detail::OtherReset;
using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegSpec;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

constexpr uint32_t kMcscrPa       = 0x1FFFFF50u;
constexpr uint32_t kMcscrCount    = 8u;
constexpr uint16_t kMcscrWritable = 0x007Fu;
constexpr uint16_t kMcscrReserved = 0xFF80u;

constexpr RegWindowModel McscrWindow() {
    RegWindowModel w{kMcscrPa, kMcscrCount * 2u, kMcscrCount, false, {}};
    for (uint32_t i = 0; i < kMcscrCount; ++i) {
        w.reg[i] = RegSpec{ReadKind::kStored, WriteKind::kStored, kMcscrWritable, 0u, kMcscrReserved,
                           OtherReset::kRetain};
    }
    return w;
}

constexpr RegWindowModel kSh7729Mcscr = McscrWindow();

class Sh7729Mcscr : public cerf_reg_window_detail::RegWindowBase<SocId::Sh7729, kSh7729Mcscr> {
public:
    using RegWindowBase::RegWindowBase;
};

}

REGISTER_SERVICE(Sh7729Mcscr);

}

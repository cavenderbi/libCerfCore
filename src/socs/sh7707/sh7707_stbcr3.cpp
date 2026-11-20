#include "../sh77xx/sh77xx_power_down_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kStbcr3Pa        = 0x1FFFFF8Au;
constexpr uint16_t kStbcr3Writable  = 0x81u;
constexpr uint16_t kStbcr3HaltOnSet = 0x7Fu;

constexpr cerf_reg_window_detail::RegWindowModel kSh7707Stbcr3 = sh77xx_power_down::ByteRegister(
    kStbcr3Pa, kStbcr3Writable, kStbcr3HaltOnSet, cerf_reg_window_detail::OtherReset::kRetain);

class Sh7707Stbcr3 : public cerf_reg_window_detail::RegWindowBase<SocId::Sh7707, kSh7707Stbcr3> {
public:
    using RegWindowBase::RegWindowBase;
};

}

REGISTER_SERVICE(Sh7707Stbcr3);

}

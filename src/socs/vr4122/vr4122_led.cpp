#include "../vr41xx/vr41xx_led_impl.h"
#include "vr4122_id.h"

namespace cerfcore {

namespace {

using cerf_vr41xx_led_detail::Vr41xxLedBase;

class Vr4122Led : public Vr41xxLedBase<SocId::Vr4122, 0x0F000180u, Vr41xxRtcxDomain::Peripheral> {
public:
    using Vr41xxLedBase::Vr41xxLedBase;
};

}  /* namespace */

REGISTER_SERVICE(Vr4122Led);

}

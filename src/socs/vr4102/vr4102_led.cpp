#include "../vr41xx/vr41xx_led_impl.h"
#include "vr4102_id.h"

namespace cerfcore {

namespace {

using cerf_vr41xx_led_detail::Vr41xxLedBase;

class Vr4102Led : public Vr41xxLedBase<SocId::Vr4102, 0x0B000240u, Vr41xxRtcxDomain::RtcIcuPmu> {
public:
    using Vr41xxLedBase::Vr41xxLedBase;
};

}  /* namespace */

REGISTER_SERVICE(Vr4102Led);

}

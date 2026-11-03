#include "../vr41xx/vr41xx_led_impl.h"
#include "vr4121_id.h"

namespace cerfcore {

namespace {

using cerf_vr41xx_led_detail::Vr41xxLedBase;

class Vr4121Led : public Vr41xxLedBase<SocId::Vr4121, 0x0B000240u, Vr41xxRtcxDomain::RtcIcuPmu> {
public:
    using Vr41xxLedBase::Vr41xxLedBase;
};

}

REGISTER_SERVICE(Vr4121Led);

}

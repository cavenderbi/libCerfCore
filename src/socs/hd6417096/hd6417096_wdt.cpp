#include "../sh77xx/sh77xx_wdt_impl.h"
#include "hd6417096_id.h"

namespace cerfcore {

namespace {

class Hd6417096Wdt : public sh77xx_wdt::Sh77xxWdtBase<SocId::Hd6417096, false> {
public:
    using Sh77xxWdtBase::Sh77xxWdtBase;
};

}

REGISTER_SERVICE_AS(Hd6417096Wdt, Sh77xxWdt);

}

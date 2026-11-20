#include "../sh77xx/sh77xx_wdt_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

class Sh7707Wdt : public sh77xx_wdt::Sh77xxWdtBase<SocId::Sh7707> {
public:
    using Sh77xxWdtBase::Sh77xxWdtBase;
};

}

REGISTER_SERVICE_AS(Sh7707Wdt, Sh77xxWdt);

}

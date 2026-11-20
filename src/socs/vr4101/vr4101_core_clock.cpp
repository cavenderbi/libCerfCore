#include "../vr41xx/vr41xx_core_clock_impl.h"
#include "../vr41xx/vr41xx_rtcx_ticks.h"
#include "vr4101_id.h"

namespace cerfcore {

namespace {

constexpr Vr41xxCoreClockModel kVr4101CoreClock{
    Vr41xxRtcxTicks::kHz,
    1012u,
    2u,
    {{1u, 4u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u},
     {0u, 0u, 0u}, {0u, 0u, 0u}},
    {kVr41xxEcReserved, kVr41xxEcReserved, 0u, kVr41xxEcReserved, kVr41xxEcReserved,
     kVr41xxEcReserved, kVr41xxEcReserved, kVr41xxEcReserved},
};

class Vr4101CoreClock : public Vr41xxCoreClockBase<SocId::Vr4101, kVr4101CoreClock> {
public:
    using Vr41xxCoreClockBase::Vr41xxCoreClockBase;

protected:
    const Vr41xxClkselRow& Row() override { return kVr4101CoreClock.rows[0]; }
};

}

REGISTER_SERVICE_AS(Vr4101CoreClock, MipsCoreClock);

}

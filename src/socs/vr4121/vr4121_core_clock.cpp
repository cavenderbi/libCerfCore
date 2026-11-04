#include "vr4121_clock_state.h"
#include "vr4121_core_clock_model.h"
#include "vr4121_id.h"

namespace cerfcore {

namespace {

class Vr4121CoreClock : public Vr41xxCoreClockBase<SocId::Vr4121, kVr4121CoreClock> {
public:
    using Vr41xxCoreClockBase::Vr41xxCoreClockBase;

protected:
    const Vr41xxClkselRow& Row() override { return emu_.Get<Vr4121ClockState>().Row(); }
};

}

REGISTER_SERVICE_AS(Vr4121CoreClock, MipsCoreClock);

}

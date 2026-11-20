#include "../sh77xx/sh77xx_rtc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7707_id.h"

namespace cerfcore {

namespace {

class Sh7707Rtc : public Sh77xxRtc {
public:
    using Sh77xxRtc::Sh77xxRtc;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }
};

}

REGISTER_SERVICE_AS(Sh7707Rtc, Sh77xxRtcOscillator);

}

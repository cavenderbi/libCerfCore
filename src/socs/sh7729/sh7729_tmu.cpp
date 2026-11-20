#include "../sh77xx/sh77xx_tmu.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7729_id.h"

namespace cerfcore {

namespace {

class Sh7729Tmu : public Sh77xxTmu {
public:
    using Sh77xxTmu::Sh77xxTmu;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7729; }

protected:
    bool StandbyInitializesFlags() const override { return false; }
};

}

REGISTER_SERVICE(Sh7729Tmu);

}

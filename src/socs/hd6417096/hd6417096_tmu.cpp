#include "../sh77xx/sh77xx_tmu.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hd6417096_id.h"

namespace cerfcore {

namespace {

class Hd6417096Tmu : public Sh77xxTmu {
public:
    using Sh77xxTmu::Sh77xxTmu;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }

protected:
    bool StandbyInitializesFlags() const override { return false; }
};

}

REGISTER_SERVICE(Hd6417096Tmu);

}

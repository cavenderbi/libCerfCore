#include "../sh77xx/sh77xx_sci.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hd6417096_id.h"

namespace cerfcore {

namespace {

class Hd6417096Sci : public Sh77xxSci {
public:
    using Sh77xxSci::Sh77xxSci;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }
};

}

REGISTER_SERVICE(Hd6417096Sci);

}

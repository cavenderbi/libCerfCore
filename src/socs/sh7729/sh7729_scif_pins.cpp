#include "../sh77xx/sh77xx_port_scif_pins.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7729_id.h"

namespace cerfcore {

namespace {

class Sh7729ScifPins : public Sh77xxPortScifPins {
public:
    using Sh77xxPortScifPins::Sh77xxPortScifPins;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7729; }
};

}

REGISTER_SERVICE_AS(Sh7729ScifPins, Sh77xxScifPins);

}

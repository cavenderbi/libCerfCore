#include "../sh77xx/sh77xx_port_scif_pins.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7707_id.h"

namespace cerfcore {

namespace {

class Sh7707ScifPins : public Sh77xxPortScifPins {
public:
    using Sh77xxPortScifPins::Sh77xxPortScifPins;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }
};

}

REGISTER_SERVICE_AS(Sh7707ScifPins, Sh77xxScifPins);

}

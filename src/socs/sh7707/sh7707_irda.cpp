#include "../sh77xx/sh77xx_scif.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7707_id.h"

namespace cerfcore {

namespace {

class Sh7707Irda : public Sh77xxScif {
public:
    using Sh77xxScif::Sh77xxScif;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }

protected:
    const Sh77xxScifChannel& Channel() const override { return kSh77xxIrdaChannel; }
};

}

REGISTER_SERVICE(Sh7707Irda);

}

#include "../sh77xx/sh77xx_cmt.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7707_id.h"

namespace cerfcore {

namespace {

class Sh7707Cmt : public Sh77xxCmt {
public:
    using Sh77xxCmt::Sh77xxCmt;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }

protected:
    uint16_t CmstrWriteZeroBits() const override { return 0u; }
    uint16_t CmcsrWriteZeroBits() const override { return 0u; }
};

}

REGISTER_SERVICE(Sh7707Cmt);

}

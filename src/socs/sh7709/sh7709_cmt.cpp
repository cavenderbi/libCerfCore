#include "../sh77xx/sh77xx_cmt.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7709_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kCmstrWriteZeroBits = 0x0002u;
constexpr uint16_t kCmcsrWriteZeroBits = 0x0040u;

class Sh7709Cmt : public Sh77xxCmt {
public:
    using Sh77xxCmt::Sh77xxCmt;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7709; }

protected:
    uint16_t CmstrWriteZeroBits() const override { return kCmstrWriteZeroBits; }
    uint16_t CmcsrWriteZeroBits() const override { return kCmcsrWriteZeroBits; }
};

}

REGISTER_SERVICE(Sh7709Cmt);

}

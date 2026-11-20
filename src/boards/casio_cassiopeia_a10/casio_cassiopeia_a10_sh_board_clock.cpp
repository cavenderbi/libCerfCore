#include "../../cpu/sh/sh_board_clock.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10ShBoardClock : public ShBoardClock {
public:
    using ShBoardClock::ShBoardClock;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    uint64_t CkioHz() const override { return 10000000u; }
    bool     Pll1Switchable() const override { return true; }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10ShBoardClock, ShBoardClock);

}

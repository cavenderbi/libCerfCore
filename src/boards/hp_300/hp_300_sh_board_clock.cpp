#include "../../cpu/sh/sh_board_clock.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

class Hp300ShBoardClock : public ShBoardClock {
public:
    using ShBoardClock::ShBoardClock;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    uint64_t CkioHz() const override { return 11000000u; }
    bool     Pll1Switchable() const override { return true; }
};

}

REGISTER_SERVICE_AS_FALLBACK(Hp300ShBoardClock, ShBoardClock);

}

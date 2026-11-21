#include "../../cpu/sh/sh_board_clock.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

class HpJornada540ShBoardClock : public ShBoardClock {
public:
    using ShBoardClock::ShBoardClock;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

    uint64_t CkioHz() const override { return 22118400u; }
    bool     Pll1Switchable() const override { return false; }
};

}

REGISTER_SERVICE_AS(HpJornada540ShBoardClock, ShBoardClock);

}

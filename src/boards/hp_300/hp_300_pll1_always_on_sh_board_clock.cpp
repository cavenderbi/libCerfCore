#include "../../cpu/sh/sh_board_clock.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_300_family_id.h"
#include "hp_300_member_straps.h"

namespace cerfcore {

namespace {

class Hp300Pll1AlwaysOnShBoardClock : public ShBoardClock {
public:
    using ShBoardClock::ShBoardClock;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300 &&
               !emu_.Get<Hp300MemberStraps>().Luke();
    }

    uint64_t CkioHz() const override { return 30000000u; }
    bool     Pll1Switchable() const override { return false; }
};

}

REGISTER_SERVICE_AS(Hp300Pll1AlwaysOnShBoardClock, ShBoardClock);

}

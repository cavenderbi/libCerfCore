#include "hp_300_member_straps.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_300lx_id.h"

namespace cerfcore {

namespace {

class Hp300LukeModel1MemberStraps : public Hp300FixedMemberStraps<true, false> {
public:
    using Hp300FixedMemberStraps::Hp300FixedMemberStraps;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardId() == BoardId::Hp300lx;
    }
};

}

REGISTER_SERVICE_AS(Hp300LukeModel1MemberStraps, Hp300MemberStraps);

}

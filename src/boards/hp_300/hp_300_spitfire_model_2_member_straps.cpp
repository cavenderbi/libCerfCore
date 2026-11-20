#include "hp_300_member_straps.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "ericsson_mc16_id.h"
#include "hp_360lx_id.h"

namespace cerfcore {

namespace {

class Hp300SpitfireModel2MemberStraps : public Hp300FixedMemberStraps<false, true> {
public:
    using Hp300FixedMemberStraps::Hp300FixedMemberStraps;

    bool ShouldRegister() override {
        const std::string_view board = emu_.Get<BoardContext>().GetBoardId();
        return board == BoardId::Hp360lx || board == BoardId::EricssonMc16;
    }
};

}

REGISTER_SERVICE_AS(Hp300SpitfireModel2MemberStraps, Hp300MemberStraps);

}

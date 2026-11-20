#include "hp_300_member_straps.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

class Hp300LukeModel2MemberStraps : public Hp300FixedMemberStraps<true, true> {
public:
    using Hp300FixedMemberStraps::Hp300FixedMemberStraps;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }
};

}

REGISTER_SERVICE_AS_FALLBACK(Hp300LukeModel2MemberStraps, Hp300MemberStraps);

}

#include "../../peripherals/hp_debug_board/hp_debug_board_placement.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

class Hp300DebugBoardPlacement : public HpDebugBoardPlacement {
public:
    using HpDebugBoardPlacement::HpDebugBoardPlacement;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    uint32_t BasePa() const override { return 0x02000000u; }
    uint32_t Cd1284RegisterShift() const override { return 1u; }
    uint32_t PostLatchOffset() const override { return 0x100u; }
};

}

REGISTER_SERVICE_AS(Hp300DebugBoardPlacement, HpDebugBoardPlacement);

}

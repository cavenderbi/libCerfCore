#include "../../peripherals/hp_debug_board/hp_debug_board_placement.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7709/sh7709_id.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

class Hp600Sh7709DebugBoardPlacement : public HpDebugBoardPlacement {
public:
    using HpDebugBoardPlacement::HpDebugBoardPlacement;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7709;
    }

    uint32_t BasePa() const override { return 0x02000000u; }
    uint32_t Cd1284RegisterShift() const override { return 2u; }
    uint32_t PostLatchOffset() const override { return 0x400u; }
};

}

REGISTER_SERVICE_AS(Hp600Sh7709DebugBoardPlacement, HpDebugBoardPlacement);

}

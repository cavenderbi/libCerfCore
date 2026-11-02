#include "../nec_vr41xx_isa_rf5c396_board.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_cronos3e_family_id.h"

namespace cerfcore {

namespace {

class NecCronos3eRf5c396Board : public NecVr41xxIsaRf5c396Board {
public:
    using NecVr41xxIsaRf5c396Board::NecVr41xxIsaRf5c396Board;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecCronos3e;
    }
};

}

REGISTER_SERVICE_AS(NecCronos3eRf5c396Board, RicohRf5c396Board);

}

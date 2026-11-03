#include "../nec_vr41xx_keyboard_input.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_mobilepro_780_id.h"
#include "nec_mobilepro_790_id.h"

namespace cerfcore {

namespace {

class NecCronos3eKeyboardInput : public NecVr41xxKeyboardInput {
public:
    using NecVr41xxKeyboardInput::NecVr41xxKeyboardInput;

    bool ShouldRegister() override {
        const auto board = emu_.Get<BoardContext>().GetBoardId();
        return board == BoardId::NecMobilepro790 || board == BoardId::NecMobilepro780;
    }
};

}

REGISTER_SERVICE(NecCronos3eKeyboardInput);

}

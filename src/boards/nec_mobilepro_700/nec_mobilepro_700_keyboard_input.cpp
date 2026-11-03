#include "../nec_vr41xx_keyboard_input.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_mobilepro_700_family_id.h"

namespace cerfcore {

namespace {

class NecMobilepro700KeyboardInput : public NecVr41xxKeyboardInput {
public:
    using NecVr41xxKeyboardInput::NecVr41xxKeyboardInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro700;
    }
};

}

REGISTER_SERVICE(NecMobilepro700KeyboardInput);

}

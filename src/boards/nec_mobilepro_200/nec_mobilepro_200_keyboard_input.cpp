#include "../nec_vr41xx_keyboard_input.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_mobilepro_200_family_id.h"

namespace cerfcore {

namespace {

class NecMobilepro200KeyboardInput : public NecVr41xxKeyboardInput {
public:
    using NecVr41xxKeyboardInput::NecVr41xxKeyboardInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro200;
    }
};

}

REGISTER_SERVICE(NecMobilepro200KeyboardInput);

}

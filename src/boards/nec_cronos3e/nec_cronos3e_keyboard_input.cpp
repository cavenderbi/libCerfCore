#include "../nec_vr41xx_keyboard_input.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/vr41xx/vr41xx_giu.h"
#include "nec_mobilepro_780_id.h"
#include "nec_mobilepro_790_id.h"

namespace cerfcore {

namespace {

constexpr int kRecordButtonGpio = 2;

class NecCronos3eKeyboardInput : public NecVr41xxKeyboardInput {
public:
    using NecVr41xxKeyboardInput::NecVr41xxKeyboardInput;

    bool ShouldRegister() override {
        const auto board = emu_.Get<BoardContext>().GetBoardId();
        return board == BoardId::NecMobilepro790 || board == BoardId::NecMobilepro780;
    }

protected:
    void OnBoardKey(uint32_t code, bool pressed) override {
        if (code == kNecVr41xxRecordButtonCode) {
            emu_.Get<Vr41xxGiu>().SetPinLevel(kRecordButtonGpio, pressed);
            return;
        }
        NecVr41xxKeyboardInput::OnBoardKey(code, pressed);
    }
};

}

REGISTER_SERVICE(NecCronos3eKeyboardInput);

}

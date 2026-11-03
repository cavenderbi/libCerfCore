#include "nec_vr41xx_keyboard_input.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../socs/vr41xx/vr41xx_pmu.h"

namespace cerfcore {

void NecVr41xxKeyboardInput::OnBoardKey(uint32_t, bool pressed) {
    emu_.Get<Vr41xxPmu>().SetPowerSwitch(pressed);
}

}

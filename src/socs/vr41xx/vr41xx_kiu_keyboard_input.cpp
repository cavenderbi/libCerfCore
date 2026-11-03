#include "vr41xx_kiu_keyboard_input.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/host/keyboard_router.h>
#include "vr41xx_kiu.h"

namespace cerfcore {

void Vr41xxKiuKeyboardInput::OnReady() {
    emu_.Get<KeyboardRouter>().Register(this);
}

void Vr41xxKiuKeyboardInput::OnHostKey(uint8_t vk, bool key_up) {
    uint32_t code;
    if (!emu_.Get<KeyboardMap>().BaseDeviceCode(vk, code)) return;
    if (code < kVr41xxKiuBoardKeyBase) emu_.Get<Vr41xxKiu>().SetKeyState(static_cast<uint8_t>(code), !key_up);
    else                               OnBoardKey(code, !key_up);
}

}

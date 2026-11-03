#pragma once

#include "../socs/vr41xx/vr41xx_kiu_keyboard_input.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kNecVr41xxPowerSwitchCode  = kVr41xxKiuBoardKeyBase;
inline constexpr uint32_t kNecVr41xxRecordButtonCode = kVr41xxKiuBoardKeyBase + 1u;

class NecVr41xxKeyboardInput : public Vr41xxKiuKeyboardInput {
public:
    using Vr41xxKiuKeyboardInput::Vr41xxKiuKeyboardInput;

protected:
    void OnBoardKey(uint32_t code, bool pressed) override;
};

}

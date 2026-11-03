#pragma once

#include <cerfcore/host/keyboard_input.h>

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kVr41xxKiuBoardKeyBase = 0x100u;

class Vr41xxKiuKeyboardInput : public KeyboardInput {
public:
    using KeyboardInput::KeyboardInput;

    void OnReady() override;
    void OnHostKey(uint8_t vk, bool key_up) override;

protected:
    virtual void OnBoardKey(uint32_t code, bool pressed) = 0;
};

}

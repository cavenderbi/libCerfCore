#pragma once

#include <cerfcore/host/keyboard_map.h>

#include <cstdint>
#include <vector>

namespace cerfcore {

class MatrixKeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    const std::vector<KeyBinding>& Bindings() const override { return bindings_; }

protected:
    void AddCell(uint8_t vk, uint32_t cell, const char* label, uint8_t holds_layer);
    void AddBinding(const KeyBinding& binding);
    void AddLayerLabel(uint8_t vk, const char* label, uint8_t layer);

private:
    std::vector<KeyBinding> bindings_;
};

}

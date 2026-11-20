#include "matrix_keyboard_map.h"

#include <cerfcore/host/virtual_keys.h>

namespace cerfcore {

namespace {

struct GenericModifier {
    uint8_t vk;
    uint8_t sided_vk;
};

constexpr GenericModifier kGenericModifiers[] = {
    {kVkShift, kVkLShift},
    {kVkControl, kVkLControl},
    {kVkMenu, kVkLMenu},
};

}

void MatrixKeyboardMap::AddCell(uint8_t vk, uint32_t cell, const char* label, uint8_t holds_layer) {
    bindings_.push_back({vk, cell, label, 0u, holds_layer});
    for (const GenericModifier& m : kGenericModifiers) {
        if (m.sided_vk == vk) bindings_.push_back({m.vk, cell, nullptr, 0u, holds_layer});
    }
}

void MatrixKeyboardMap::AddBinding(const KeyBinding& binding) { bindings_.push_back(binding); }

void MatrixKeyboardMap::AddLayerLabel(uint8_t vk, const char* label, uint8_t layer) {
    bindings_.push_back({vk, 0u, label, layer, 0u});
}

}

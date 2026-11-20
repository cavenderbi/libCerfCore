#include "hp_600_keyboard_layout.h"

namespace cerfcore {

bool Hp600KeyboardLayout::Scanned(uint32_t cell) const {
    const uint16_t scanned = ScannedColumns()[cell / Sh77xxPortKeyMatrix::kColumns];
    return ((scanned >> (cell % Sh77xxPortKeyMatrix::kColumns)) & 1u) != 0u;
}

std::optional<uint32_t> Hp600KeyboardLayout::ScannedCellOf(uint8_t vk) const {
    for (uint32_t cell = 0; cell < kHp600KeyCells; ++cell) {
        if (CellVk()[cell] == vk && Scanned(cell)) return cell;
    }
    return std::nullopt;
}

}

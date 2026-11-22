#include "hp_600_keyboard_layout.h"

namespace cerfcore {

bool Hp600KeyboardLayout::Scanned(uint32_t cell) const {
    const uint16_t scanned = ScannedColumns()[cell / Sh77xxPortKeyMatrix::kColumns];
    return ((scanned >> (cell % Sh77xxPortKeyMatrix::kColumns)) & 1u) != 0u;
}

std::optional<uint32_t> Hp600KeyboardLayout::LedButtonCell() const {
    for (uint32_t row = 0; row < Sh77xxPortKeyMatrix::kRows; ++row) {
        const uint32_t cell = row * Sh77xxPortKeyMatrix::kColumns + kHp600PortC7Column;
        if (Scanned(cell)) return cell;
    }
    return std::nullopt;
}

}

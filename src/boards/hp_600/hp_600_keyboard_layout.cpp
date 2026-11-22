#include "hp_600_keyboard_layout.h"

namespace cerfcore {

bool Hp600KeyboardLayout::Scanned(uint32_t cell) const {
    const uint16_t scanned = ScannedColumns()[cell / Sh77xxPortKeyMatrix::kColumns];
    return ((scanned >> (cell % Sh77xxPortKeyMatrix::kColumns)) & 1u) != 0u;
}

}

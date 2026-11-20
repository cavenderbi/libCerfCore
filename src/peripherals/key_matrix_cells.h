#pragma once

#include <cstdint>

namespace cerfcore {

template <uint32_t kLines>
class KeyMatrixCells {
public:
    static constexpr uint32_t kColumns = 16u;

    bool Set(uint32_t cell, bool down) {
        const uint32_t line = cell / kColumns;
        const uint16_t bit  = static_cast<uint16_t>(1u << (cell % kColumns));
        const uint16_t next = down ? static_cast<uint16_t>(pressed_[line] | bit) : static_cast<uint16_t>(pressed_[line] & ~bit);
        if (next == pressed_[line]) return false;
        pressed_[line] = next;
        return true;
    }

    uint16_t Line(uint32_t line) const { return pressed_[line]; }
    void     SetLine(uint32_t line, uint16_t value) { pressed_[line] = value; }

    uint16_t Pressed(uint32_t driven_lines) const {
        uint16_t columns = 0u;
        for (uint32_t line = 0; line < kLines; ++line) {
            if (((driven_lines >> line) & 1u) != 0u) columns = static_cast<uint16_t>(columns | pressed_[line]);
        }
        return columns;
    }

    void ReleaseAll() {
        for (uint16_t& line : pressed_) line = 0u;
    }

    bool operator==(const KeyMatrixCells&) const = default;

private:
    uint16_t pressed_[kLines] = {};
};

}

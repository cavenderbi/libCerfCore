#include <cerfcore/peripherals/cerf_virt/guest_cursor_pixels.h>

#include <cerfcore/lcd/lcd_pixel_expand.h>
#include <cerfcore/peripherals/cerf_virt/cerf_virt_cursor.h>

namespace cerfcore {

std::vector<CursorPixel> GuestCursorPixels::Decode(const GuestCursorShape& s) {
    std::vector<CursorPixel> out(static_cast<size_t>(s.cx) * s.cy, CursorPixel::kTransparent);
    for (uint32_t row = 0; row < s.cy; ++row) {
        const uint8_t* and_row = s.bits.data() + static_cast<size_t>(row) * s.stride;
        const uint8_t* xor_row = s.bits.data() + static_cast<size_t>(s.cy + row) * s.stride;
        for (uint32_t col = 0; col < s.cx; ++col) {
            const bool and_bit = lcd_pixel::PackedIndexMsbFirst(and_row, col, 1u) != 0u;
            const bool xor_bit = lcd_pixel::PackedIndexMsbFirst(xor_row, col, 1u) != 0u;
            CursorPixel& px    = out[static_cast<size_t>(row) * s.cx + col];
            if (and_bit) px = xor_bit ? CursorPixel::kInvert : CursorPixel::kTransparent;
            else         px = xor_bit ? CursorPixel::kWhite : CursorPixel::kBlack;
        }
    }
    return out;
}

}

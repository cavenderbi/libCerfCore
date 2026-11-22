#include "epson_ink_cursor_layer.h"

#include <cerfcore/lcd/lcd_pixel_expand.h>
#include "../epson_display_buffer/epson_display_buffer.h"

#include <cstddef>

namespace cerfcore {

uint32_t EpsonInkCursorLayer::StartByte(uint32_t buffer_bytes, uint8_t select) {
    return select == 0u ? buffer_bytes - 1024u : buffer_bytes - select * 8192u;
}

void EpsonInkCursorLayer::ComposeInto(const EpsonDisplayBuffer& buffer, uint32_t* dib, uint32_t host_w,
                                      uint32_t cw, uint32_t ch) const {
    for (uint32_t iy = 0; iy < height; ++iy) {
        const int32_t dy = y + static_cast<int32_t>(iy);
        if (dy < 0 || static_cast<uint32_t>(dy) >= ch) continue;
        for (uint32_t ix = 0; ix < width; ++ix) {
            const int32_t dx = x + static_cast<int32_t>(ix);
            if (dx < 0 || static_cast<uint32_t>(dx) >= cw) continue;
            const uint32_t byte = buffer.Load(start_byte + iy * line_bytes + ix / 4u, 1u);
            uint32_t*      d    = dib + static_cast<size_t>(dy) * host_w + dx;
            switch (lcd_pixel::IndexMsbFirst(byte, ix * 2u, 2u)) {
                case 0u: *d = color0; break;
                case 1u: *d = color1; break;
                case 2u: break;
                default: *d = 0xFF000000u | ~*d; break;
            }
        }
    }
}

}

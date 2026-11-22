#pragma once

#include <cstdint>

namespace cerfcore {

class EpsonDisplayBuffer;

struct EpsonInkCursorLayer {
    int32_t  x          = 0;
    int32_t  y          = 0;
    uint32_t width      = 0;
    uint32_t height     = 0;
    uint32_t start_byte = 0;
    uint32_t line_bytes = 0;
    uint32_t color0     = 0;
    uint32_t color1     = 0;

    static uint32_t StartByte(uint32_t buffer_bytes, uint8_t select);

    void ComposeInto(const EpsonDisplayBuffer& buffer, uint32_t* dib, uint32_t host_w, uint32_t cw,
                     uint32_t ch) const;
};

}

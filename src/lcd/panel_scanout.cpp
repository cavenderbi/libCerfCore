#include "panel_scanout.h"

#include <cerfcore/lcd/lcd_pixel_expand.h>
#include <cerfcore/core/byte_order.h>

#include <cstring>

namespace cerfcore {

uint32_t PanelScanout::GrayIndex(const uint8_t* line, size_t plane_bytes, uint32_t x) const {
    switch (layout_) {
        case PanelGrayLayout::kPacked2Msb:
            return lcd_pixel::PackedIndexMsbFirst(line, x, 2u);
        case PanelGrayLayout::kPlanes2Lsb:
            return lcd_pixel::PackedIndexLsbFirst(line, x, 1u) |
                   lcd_pixel::PackedIndexLsbFirst(line + plane_bytes, x, 1u) << 1;
    }
    return 0u;
}

void PanelScanout::Blit(const PanelSurface& src, uint32_t* dib,
                        uint32_t host_w, uint32_t host_h) const {
    std::memset(dib, 0, static_cast<size_t>(host_w) * host_h * 4u);

    const uint32_t cw = (host_w < src.width)  ? host_w : src.width;
    const uint32_t ch = (host_h < src.height) ? host_h : src.height;

    const size_t plane_bytes = static_cast<size_t>(src.stride) * src.height;

    for (uint32_t y = 0; y < ch; ++y) {
        const uint8_t* line = src.fb + static_cast<size_t>(y) * src.stride;
        uint32_t*      dst  = dib + static_cast<size_t>(y) * host_w;
        if (gray_) {
            for (uint32_t x = 0; x < cw; ++x) {
                const uint32_t g = levels_[GrayIndex(line, plane_bytes, x)];
                dst[x] = lcd_pixel::PackXrgb(g, g, g);
            }
            continue;
        }
        switch (format_) {
            case PanelPixelFormat::kRgb565Le:
                for (uint32_t x = 0; x < cw; ++x)
                    dst[x] = lcd_pixel::Expand565(cerf::le::U16(line, x * 2u));
                break;
        }
    }
}

}

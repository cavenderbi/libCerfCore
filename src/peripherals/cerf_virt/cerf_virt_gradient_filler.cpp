#include "cerf_virt_gradient_filler.h"
#include "cerf_virt_blt_pixelops.h"
#include <cerfcore/peripherals/cerf_virt/cerf_virt_framebuffer.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/log.h>

#include <vector>

namespace cerfcore {

namespace CerfVirt {

REGISTER_SERVICE(CerfVirtGradientFiller);

bool CerfVirtGradientFiller::ShouldRegister() {
    return emu_.Get<DeviceConfig>().guest_additions;
}

void CerfVirtGradientFiller::OnReady() {
    fb_  = &emu_.Get<CerfVirtFramebuffer>();
    arena_  = &emu_.Get<CerfVirtDmaArena>();
    fatal_  = &emu_.Get<Fatal>();
}

namespace {

int64_t GradFloorDiv(int64_t num, int64_t span) {
    int64_t q = num / span;
    if (num % span != 0 && num < 0) --q;
    return q;
}

uint32_t GradientLevel(int64_t base, int64_t step, int32_t t) {
    return (uint8_t)((base + step * (int64_t)t) >> 48);
}

constexpr uint8_t kIndexedDither[16][16] = {
    {0x08, 0x48, 0x18, 0x58, 0x0C, 0x4C, 0x1C, 0x5C, 0x09, 0x49, 0x19, 0x59, 0x0D, 0x4D, 0x1D, 0x5D},
    {0x68, 0x28, 0x78, 0x38, 0x6C, 0x2C, 0x7C, 0x3C, 0x69, 0x29, 0x79, 0x39, 0x6D, 0x2D, 0x7D, 0x3D},
    {0x10, 0x50, 0x00, 0x40, 0x14, 0x54, 0x04, 0x44, 0x11, 0x51, 0x01, 0x41, 0x15, 0x55, 0x05, 0x45},
    {0x70, 0x30, 0x60, 0x20, 0x74, 0x34, 0x64, 0x24, 0x71, 0x31, 0x61, 0x21, 0x75, 0x35, 0x65, 0x25},
    {0x0E, 0x4E, 0x1E, 0x5E, 0x0A, 0x4A, 0x1A, 0x5A, 0x0F, 0x4F, 0x1F, 0x5F, 0x0B, 0x4B, 0x1B, 0x5B},
    {0x6E, 0x2E, 0x7E, 0x3E, 0x6A, 0x2A, 0x7A, 0x3A, 0x6F, 0x2F, 0x7E, 0x3F, 0x6B, 0x2B, 0x7B, 0x3B},
    {0x16, 0x56, 0x06, 0x46, 0x12, 0x52, 0x02, 0x42, 0x17, 0x57, 0x07, 0x47, 0x13, 0x53, 0x03, 0x43},
    {0x76, 0x36, 0x66, 0x26, 0x72, 0x32, 0x62, 0x22, 0x77, 0x37, 0x67, 0x27, 0x73, 0x33, 0x63, 0x23},
    {0x09, 0x49, 0x19, 0x59, 0x0D, 0x4D, 0x1D, 0x5D, 0x08, 0x48, 0x18, 0x58, 0x0C, 0x4C, 0x1C, 0x5C},
    {0x69, 0x29, 0x79, 0x39, 0x6D, 0x2D, 0x7D, 0x3D, 0x68, 0x28, 0x78, 0x38, 0x6C, 0x2C, 0x7C, 0x3C},
    {0x11, 0x51, 0x01, 0x41, 0x15, 0x55, 0x05, 0x45, 0x10, 0x50, 0x00, 0x40, 0x14, 0x54, 0x04, 0x44},
    {0x71, 0x31, 0x61, 0x21, 0x75, 0x35, 0x65, 0x25, 0x70, 0x30, 0x60, 0x20, 0x74, 0x34, 0x64, 0x24},
    {0x0F, 0x4F, 0x1F, 0x5F, 0x0B, 0x4B, 0x1B, 0x5B, 0x0E, 0x4E, 0x1E, 0x5E, 0x0A, 0x4A, 0x1A, 0x5A},
    {0x6F, 0x2F, 0x7E, 0x3F, 0x6B, 0x2B, 0x7B, 0x3B, 0x6E, 0x2E, 0x7E, 0x3E, 0x6A, 0x2A, 0x7A, 0x3A},
    {0x17, 0x57, 0x07, 0x47, 0x13, 0x53, 0x03, 0x43, 0x16, 0x56, 0x06, 0x46, 0x12, 0x52, 0x02, 0x42},
    {0x77, 0x37, 0x67, 0x27, 0x73, 0x33, 0x63, 0x23, 0x76, 0x36, 0x66, 0x26, 0x72, 0x32, 0x62, 0x22},
};

uint32_t DitheredLevel(uint32_t level, uint32_t dither, uint32_t bits) {
    if (bits == 1u) return (level + 2u * dither >= 0xFFu) ? 0xFFu : 0x00u;
    const uint32_t sum = level + dither;
    return sum < 0x80u ? 0x00u : (sum < 0xFFu ? 0x80u : 0xFFu);
}

}

bool CerfVirtGradientFiller::FillIndexed(const CerfGradDescriptor& g, uint32_t d_bits, int32_t span,
                                         const int64_t base[4], const int64_t step[4]) {
    if (d_bits == 0u)
        fatal_->Die("[CerfVirtGradientFiller] UNIMPLEMENTED dst format %u", g.dst.format);
    LoadIndexConverter(&index_converter_, g.dst_palette_off, g.dst_palette_entries);
    Surface dst;
    if (!ResolveSurface(g.dst, 1u, &dst)) {
        fatal_->Die("[CerfVirtGradientFiller] indexed dst surface unresolvable: buffer=0x%08X "
                    "is_fb_pa=%u fmt=%u stride=%d stage_off=0x%08X stage_len=%u",
                    g.dst.buffer, g.dst.is_fb_pa, g.dst.format, g.dst.stride,
                    g.dst.stage_off, g.dst.stage_len);
    }
    const CerfBltRect& r = g.fill_rect;
    const bool    horiz  = (g.axis == kCerfGradAxisH);
    const int32_t n      = horiz ? (r.right - r.left) : (r.bottom - r.top);
    if (n <= 0) return true;
    const int32_t origin = horiz ? r.left : r.top;
    std::vector<uint32_t> levels((size_t)n * 3u);
    for (int32_t i = 0; i < n; ++i) {
        int32_t t = (origin + i) - g.start_coord;
        if (t < 0) t = 0; else if (t > span) t = span;
        for (int c = 0; c < 3; ++c) levels[(size_t)i * 3u + (size_t)c] = GradientLevel(base[c], step[c], t);
    }
    for (int32_t y = r.top; y < r.bottom; ++y) {
        if (g.band_y_count != 0u &&
            ((uint32_t)y < g.band_y_first ||
             (uint32_t)y >= g.band_y_first + g.band_y_count)) continue;
        const uint8_t* dither_row = kIndexedDither[(uint32_t)(g.dither_y + y) & 15u];
        for (int32_t x = r.left; x < r.right; ++x) {
            const uint32_t* level  = &levels[(size_t)(horiz ? x - r.left : y - r.top) * 3u];
            const uint32_t  dither = dither_row[(uint32_t)(g.dither_x + x) & 15u];
            uint32_t colorref = 0u;
            for (uint32_t c = 0; c < 3u; ++c) colorref |= DitheredLevel(level[c], dither, d_bits) << (8u * c);
            const uint32_t index = index_converter_.Convert(colorref);
            if (d_bits < 8u) {
                if (WriteSubBytePixel(dst, x, y, d_bits, index)) continue;
            } else {
                uint32_t run = 0;
                if (uint8_t* dp = PixelPtr(dst, x, y, 1u, &run)) {
                    *dp = (uint8_t)index;
                    continue;
                }
            }
            fatal_->Die("[CerfVirtGradientFiller] indexed dst pixel unaddressable at x=%d y=%d: "
                        "buffer=0x%08X is_fb_pa=%u stage_off=0x%08X stage_len=%u",
                        x, y, g.dst.buffer, g.dst.is_fb_pa, g.dst.stage_off, g.dst.stage_len);
        }
    }
    fb_->MarkDirty();
    return true;
}

bool CerfVirtGradientFiller::Execute(const CerfGradDescriptor& g) {
    if (g.magic != kCerfGradMagic) {
        fatal_->Die("[CerfVirtGradientFiller] corrupt gradient magic 0x%08X", g.magic);
    }
    const int32_t span = g.end_coord - g.start_coord;
    if (span <= 0) return true;

    int64_t base[4], step[4];
    for (int c = 0; c < 4; ++c) {
        base[c] = (int64_t)((uint32_t)g.start_color[c] << 8) << 32;
        const int64_t delta = ((int64_t)g.end_color[c] << 8) - ((int64_t)g.start_color[c] << 8);
        step[c] = GradFloorDiv(delta << 32, span);
    }

    const uint32_t d_bits = BltPixelOps::FormatBits(g.dst.format);
    if (d_bits <= 8u) return FillIndexed(g, d_bits, span, base, step);
    uint32_t d_masks[3], d_bpp = 0, d_shift[3];
    if (!ResolveMasks(g.dst, d_masks, &d_bpp)) {
        fatal_->Die("[CerfVirtGradientFiller] UNIMPLEMENTED dst format %u (%u bpp)",
                    g.dst.format, d_bits);
    }
    for (int i = 0; i < 3; ++i) d_shift[i] = 32u - BltPixelOps::HighBitPos(d_masks[i]);

    const uint32_t a_mask  = (g.dst.pal_entries == 4u) ? g.dst.mask[3] : 0u;
    const uint32_t a_shift = a_mask ? 32u - BltPixelOps::HighBitPos(a_mask) : 0u;

    Surface dst;
    if (!ResolveSurface(g.dst, d_bpp, &dst)) {
        fatal_->Die("[CerfVirtGradientFiller] dst surface unresolvable: buffer=0x%08X "
                    "is_fb_pa=%u fmt=%u stride=%d stage_off=0x%08X stage_len=%u",
                    g.dst.buffer, g.dst.is_fb_pa, g.dst.format, g.dst.stride,
                    g.dst.stage_off, g.dst.stage_len);
    }

    const CerfBltRect& r = g.fill_rect;
    const bool horiz = (g.axis == kCerfGradAxisH);
    const int32_t n = horiz ? (r.right - r.left) : (r.bottom - r.top);
    if (n <= 0) return true;
    const int32_t origin = horiz ? r.left : r.top;
    std::vector<uint32_t> line((size_t)n);
    for (int32_t i = 0; i < n; ++i) {
        int32_t t = (origin + i) - g.start_coord;
        if (t < 0) t = 0; else if (t > span) t = span;
        uint32_t px = 0u;
        for (int c = 0; c < 3; ++c)
            px |= ((GradientLevel(base[c], step[c], t) << 24) >> d_shift[c]) & d_masks[c];
        if (a_mask) px |= ((GradientLevel(base[3], step[3], t) << 24) >> a_shift) & a_mask;
        line[(size_t)i] = px;
    }

    for (int32_t y = r.top; y < r.bottom; ++y) {
        if (g.band_y_count != 0u &&
            ((uint32_t)y < g.band_y_first ||
             (uint32_t)y >= g.band_y_first + g.band_y_count)) continue;
        const uint32_t row_px = horiz ? 0u : line[(size_t)(y - r.top)];
        for (int32_t x = r.left; x < r.right; ++x) {
            const uint32_t px = horiz ? line[(size_t)(x - r.left)] : row_px;
            uint32_t run = 0;
            uint8_t* dp = PixelPtr(dst, x, y, d_bpp, &run);
            if (!dp) {
                fatal_->Die("[CerfVirtGradientFiller] dst pixel unaddressable at x=%d y=%d: "
                            "buffer=0x%08X is_fb_pa=%u stage_off=0x%08X stage_len=%u",
                            x, y, g.dst.buffer, g.dst.is_fb_pa, g.dst.stage_off,
                            g.dst.stage_len);
            }
            if (run >= d_bpp) BltPixelOps::WritePixel(dp, d_bpp, px);
            else              WriteStraddlePixel(dst, x, y, d_bpp, px);
        }
    }
    fb_->MarkDirty();
    return true;
}

}

}

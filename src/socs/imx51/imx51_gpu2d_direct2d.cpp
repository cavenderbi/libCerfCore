#define NOMINMAX
#include "imx51_gpu2d_direct2d.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "imx51_id.h"
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>
#include "imx51_gpu2d_rasterizer.h"
#include "imx51_gpu2d_gradw_sampler.h"
#include "imx51_gpu2d_regfile.h"
#include "imx51_gpu2d_blend.h"

#include <array>
#include <cmath>

namespace cerfcore {

namespace {

constexpr bool CopyDest16(uint32_t format) {
    return Gpu2dTextureFormat16(format) || format == 4u || format == 5u;
}

/* The hardware dither pattern is undocumented. This ordered matrix follows the
   Khronos OpenVG 1.1 reference implementation for packed-16 copies. */
constexpr uint8_t kDitherMatrix[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6},
                                        {3, 11, 1, 9}, {15, 7, 13, 5}};

std::array<uint32_t, 4> DitherWidths(uint32_t format) {
    if (format == 4u || format == 11u) return {4u, 4u, 4u, 4u};
    if (format == 5u || format == 12u) return {1u, 5u, 5u, 5u};
    return {8u, 5u, 6u, 5u};
}

uint32_t DitherQuantize(uint32_t value, uint32_t source_maximum,
                        uint32_t bits, uint32_t threshold) {
    const uint32_t scaled = value * ((1u << bits) - 1u);
    uint32_t quantized = scaled / source_maximum;
    if ((scaled % source_maximum) * 16u > threshold * source_maximum) ++quantized;
    return quantized << (8u - bits);
}

uint32_t DitherPacked16(uint32_t pixel, uint32_t format, uint32_t x, uint32_t y) {
    const auto widths = DitherWidths(format);
    const uint32_t threshold = kDitherMatrix[y & 3u][x & 3u];
    uint32_t result = 0u;
    for (uint32_t channel = 0; channel < widths.size(); ++channel) {
        const uint32_t shift = 24u - channel * 8u;
        result |= DitherQuantize((pixel >> shift) & 255u, 255u, widths[channel], threshold) << shift;
    }
    return result;
}

/* Keep the exact source channel ratios: expanding to 8 bits first can make
   an identical packed-format copy spuriously dither. */
uint32_t DitherFromPacked16(uint16_t pixel, uint32_t source_format,
                            uint32_t dest_format, uint32_t x, uint32_t y) {
    uint32_t values[4] = {1u, 0u, 0u, 0u};
    uint32_t maxima[4] = {1u, 31u, 63u, 31u};
    if (source_format == 6u) {
        values[1] = (pixel >> 11) & 31u; values[2] = (pixel >> 5) & 63u; values[3] = pixel & 31u;
    } else if (source_format == 4u || source_format == 11u) {
        maxima[0] = maxima[1] = maxima[2] = maxima[3] = 15u;
        if (source_format == 4u) {
            values[0] = (pixel >> 12) & 15u; values[1] = (pixel >> 8) & 15u;
            values[2] = (pixel >> 4) & 15u; values[3] = pixel & 15u;
        } else {
            values[0] = pixel & 15u; values[1] = (pixel >> 12) & 15u;
            values[2] = (pixel >> 8) & 15u; values[3] = (pixel >> 4) & 15u;
        }
    } else {
        maxima[0] = 1u; maxima[1] = maxima[2] = maxima[3] = 31u;
        if (source_format == 5u) {
            values[0] = (pixel >> 15) & 1u; values[1] = (pixel >> 10) & 31u;
            values[2] = (pixel >> 5) & 31u; values[3] = pixel & 31u;
        } else {
            values[0] = pixel & 1u; values[1] = (pixel >> 11) & 31u;
            values[2] = (pixel >> 6) & 31u; values[3] = (pixel >> 1) & 31u;
        }
    }
    const auto widths = DitherWidths(dest_format);
    const uint32_t threshold = kDitherMatrix[y & 3u][x & 3u];
    uint32_t result = 0u;
    for (uint32_t channel = 0; channel < widths.size(); ++channel) {
        result |= DitherQuantize(values[channel], maxima[channel], widths[channel], threshold)
                  << (24u - channel * 8u);
    }
    return result;
}

constexpr uint32_t kInput = 0x0Fu;   /* ADDR_G2D_INPUT */
constexpr uint32_t kXy    = 0xF0u;   /* ADDR_G2D_XY: Y[11:0]/X[27:16] signed, vgregs_z160.h:857 */
constexpr uint32_t kWh    = 0xF1u;   /* ADDR_G2D_WIDTHHEIGHT: HEIGHT[11:0]/WIDTH[27:16], vgregs_z160.h:850 */
constexpr uint32_t kColor = 0xFFu;   /* ADDR_G2D_COLOR */
/* G2D_CFG0/1.FORMAT[15:12] surface formats (vgenums_z160.h:156-171). */
constexpr uint32_t kG2d0565 = 6u;    /* G2D_0565: 16bpp RGB565 */
constexpr uint32_t kG2d8888 = 7u;    /* G2D_8888: 32bpp ARGB */

}  /* namespace */

uint32_t Imx51Gpu2dSrgbToLinear(uint32_t color) {
    static const auto linear = [] {
        std::array<uint8_t, 256> values{};
        for (size_t index = 0; index < values.size(); ++index) {
            const float channel = static_cast<float>(index) / 255.0f;
            const float converted = channel <= 0.04045f ? channel / 12.92f
                : std::pow((channel + 0.055f) / 1.055f, 2.4f);
            values[index] = static_cast<uint8_t>(converted * 255.0f + 0.5f);
        }
        return values;
    }();
    return (color & 0xFF000000u) | (uint32_t(linear[(color >> 16) & 255u]) << 16)
        | (uint32_t(linear[(color >> 8) & 255u]) << 8) | linear[color & 255u];
}

uint32_t Imx51Gpu2dLinearToSrgb(uint32_t color) {
    static const auto srgb = [] {
        std::array<uint8_t, 256> values{};
        for (size_t index = 0; index < values.size(); ++index) {
            const float channel = static_cast<float>(index) / 255.0f;
            const float converted = channel <= 0.0031308f ? channel * 12.92f
                : 1.055f * std::pow(channel, 1.0f / 2.4f) - 0.055f;
            values[index] = static_cast<uint8_t>(converted * 255.0f + 0.5f);
        }
        return values;
    }();
    return (color & 0xFF000000u) | (uint32_t(srgb[(color >> 16) & 255u]) << 16)
        | (uint32_t(srgb[(color >> 8) & 255u]) << 8) | srgb[color & 255u];
}

REGISTER_SERVICE(Imx51Gpu2dDirect2d);

bool Imx51Gpu2dDirect2d::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetSocId() == SocId::Imx51;
}

void Imx51Gpu2dDirect2d::Halt(const uint32_t (&regs)[0x100], const char* why,
                              uint32_t addr, uint32_t data) const {
    (void)regs;
    emu_.Get<Fatal>().Die("[GPU2D-D2D] %s at 0x%08X (value 0x%08X)", why, addr, data);
}

/* Admit ALPHABLEND's two ambient defaults: OBS_ENABLE[8] (surface-init) and
   PREMULTIPLYDST[14] (copy emitter sub_41C6C448 sets it blend-off); Copy separately
   gates PREMULTIPLYDST when blending. ROP 0x404 = surface-init default
   (sub_41C61DB4), admitted as the ops' undecoded constant. */
void Imx51Gpu2dDirect2d::CheckCommonGates(const uint32_t (&regs)[0x100], bool mask_to_alpha, bool dest_swap_rb) const {
    if (regs[0xC] & ~(mask_to_alpha ? 0xC900u : 0x4900u))  /* OBS_ENABLE, OPTIMIZE and PREMULTIPLYDST */
        Halt(regs, "direct-2D op ALPHABLEND modulation (not modeled)", 0xCu, regs[0xC]);
    if (regs[0xD] != 0x404u)
        Halt(regs, "direct-2D op ROP (not modeled)", 0xDu, regs[0xD]);
    if (regs[0xD0] & 0xC0u)  /* G2D_GRADIENT.ENABLE|ENABLE2 */
        Halt(regs, "direct-2D op gradient paint (not modeled)", 0xD0u, regs[0xD0]);
    if (regs[0x10] != 0xFFFFFFu)  /* G2D_MASK identity wrap */
        Halt(regs, "direct-2D op MASK wrap (not modeled)", 0x10u, regs[0x10]);
    const uint32_t cfg0 = regs[0x1];
    if (cfg0 & (dest_swap_rb ? 0xFF5F0000u : 0xFF7F0000u))  /* TILED[16]/SRGB[17]/SWAP fields: linear, no swap - per-op
                                FORMAT decode; STRIDESIGN[23] is part of the stride */
        Halt(regs, "direct-2D op dest swap/tile/srgb (not modeled)", 0x1u, cfg0);
    if (regs[0x0] == 0u)
        Halt(regs, "direct-2D op dest BASE0 null bind", 0x0u, 0);
}

/* Direct-2D solid rect fill (fast path sub_41C6C6DC -> emitter sub_41C620DC:
   SCISSOR+XY+WIDTHHEIGHT+COLOR under INPUT=1, blender force-disabled). */
void Imx51Gpu2dDirect2d::Fill(const uint32_t (&regs)[0x100]) {
    if (regs[0x11] & (1u << 5))
        Halt(regs, "direct-2D fill with blender enabled (not modeled)", 0x11u, regs[0x11]);
    CheckCommonGates(regs, false, true);
    if (regs[0xE] != 0u && regs[0xE] != 0xF000u)  /* G2D_CONFIG: ARGBMASK-all fill only */
        Halt(regs, "direct-2D fill CONFIG mode (not modeled)", 0xEu, regs[0xE]);
    const uint32_t cfg0 = regs[0x1];
    const uint32_t fmt = (cfg0 >> 12) & 0xFu;
    if (fmt != kG2d8888 && fmt != kG2d0565 && fmt != kG2d8888Rgba && fmt != 14u && fmt != 3u && fmt != 12u && fmt != 11u && fmt != 4u && fmt != 5u)
        Halt(regs, "direct-2D fill dest format (not modeled)", 0x1u, cfg0);
    const uint32_t xy = regs[kXy], wh = regs[kWh];
    Gpu2dFillTarget t{};
    t.dest_pa   = regs[0x0];
    t.stride_dw = imx51_g2d_regfile::StrideWords(cfg0);
    t.dest_565  = (fmt == kG2d0565);
    t.dest_5551 = (fmt == 12u);
    t.dest_4444 = (fmt == 11u);
    t.dest_argb4444 = (fmt == 4u);
    t.dest_argb1555 = (fmt == 5u);
    t.dest_a8   = (fmt == 14u || fmt == 3u);
    /* G2D scissor only: the emitter programs it to the rect itself; the VGV1
       band scissor belongs to the VG rasterizer path and may be stale here. */
    const uint32_t gsx = regs[0x8], gsy = regs[0x9];
    t.clip_l = static_cast<int32_t>(gsx & 0x7FFu);
    t.clip_r = static_cast<int32_t>((gsx >> 11) & 0x7FFu);
    t.clip_t = static_cast<int32_t>(gsy & 0x7FFu);
    t.clip_b = static_cast<int32_t>((gsy >> 11) & 0x7FFu);
    /* Direct-mode COLOR uses ARGB; the memory channel order depends on FORMAT. */
    uint32_t color = regs[kColor];
    if (cfg0 & (1u << 21)) color = (color & 0xFF00FF00u) |
        ((color & 0x00FF0000u) >> 16) | ((color & 0x000000FFu) << 16);
    t.argb = Imx51Gpu2dFillColor(color, fmt);
    emu_.Get<Imx51Gpu2dRasterizer>().FillRect(
        t, static_cast<int32_t>(xy << 4) >> 20, static_cast<int32_t>(xy << 20) >> 20,
        static_cast<int32_t>(wh << 4) >> 20, static_cast<int32_t>(wh << 20) >> 20);
}

/* Source->dest copy (sub_41C6C448 EGL swap-buffer preserve, sub_41C6B2FC texture
   preparation). Both CFG0/CFG1 = STRIDE|0x7000 (FORMAT 7, all swap fields 0), so
   the copy must NOT permute channels; the texture copy additionally un-premultiplies. */
void Imx51Gpu2dDirect2d::Copy(const uint32_t (&regs)[0x100], uint32_t sxy) {
    if (regs[kInput] != 0u && regs[kInput] != 2u)
        Halt(regs, "SXY copy under unmodeled G2D_INPUT", kInput, regs[kInput]);
    const uint32_t dest_format = (regs[0x1] >> 12) & 15u;
    if (dest_format == 3u || dest_format == 14u) {
        CopyMask(regs, sxy);
        return;
    }
    const bool mask_source = regs[0xE] == 4u && (regs[0xC] & 0x8000u);
    const bool blend = (regs[0x11] & (1u << 5)) != 0u;
    const bool premultiply = blend && (regs[0x11] & ~0x100u) == 0x20u &&
        regs[0x14] == 0x00012010u && regs[0x18] == 0x00090110u;
    const bool opaque = blend && (regs[0x11] & ~0x100u) == 0x20u &&
        regs[0x14] == 0x00003010u && regs[0x18] == 0x00012010u;
    const bool unpremultiply = blend && !premultiply && !opaque;
    /* sync_2 libOpenVG.dll FUN_41c58bd4: TEMP0=SOURCE*1, OOALPHA. */
    if (unpremultiply && (regs[0x11] != 0x60u || regs[0x14] != 0x00012010u ||
                         regs[0x18] != 0x00012010u || (regs[0xC] & 0x4000u)))
        Halt(regs, "SXY copy blend program (not SOURCE with OOALPHA)", 0x11u, regs[0x11]);
    if ((regs[0xE] & ~0x400u) != 2u && (regs[0xE] & ~0x400u) != 0x802u && !mask_source)  /* G2D_CONFIG SRC1-only: DST[0]=0/SRC1[1]=1, no SRC2/SRC3/
                             colorkey/rotate/argbmask (vgregs_z160.h:2636-2655) */
        Halt(regs, "SXY copy CONFIG mode (not SRC1-only)", 0xEu, regs[0xE]);
    CheckCommonGates(regs, mask_source, true);
    const uint32_t cfg0 = regs[0x1], cfg1 = mask_source ? regs[0x5] : regs[0x3];
    const uint32_t dst_format = (cfg0 >> 12) & 15u;
    if (dst_format != kG2d8888 && dst_format != kG2d8888Rgba && !CopyDest16(dst_format))
        Halt(regs, "SXY copy dest format (not modeled)", 0x1u, cfg0);
    const uint32_t src_format = (cfg1 >> 12) & 0xFu;
    if ((src_format != kG2d8888 && src_format != kG2d8888Rgba && !Gpu2dTextureFormat1(src_format) && !Gpu2dTextureFormat16(src_format) && !Gpu2dTextureFormat8(src_format) &&
         !(mask_source && Gpu2dTextureFormat4(src_format))) || (cfg1 & 0xFF5D0000u))
        Halt(regs, "SXY copy source format (not linear G2D_8888/G2D_8888_RGBA)", 0x3u, cfg1);
    if ((mask_source ? regs[0x4] : regs[0x2]) == 0u || (mask_source && (blend && !premultiply && !opaque)))
        Halt(regs, "SXY copy source binding", 0x2u, sxy);
    const uint32_t xy = regs[kXy], wh = regs[kWh];
    Gpu2dCopySpec c{};
    c.dst_format    = dst_format;
    c.dst_pa        = regs[0x0];
    c.dst_stride_dw = imx51_g2d_regfile::StrideWords(cfg0);
    c.src_pa        = mask_source ? regs[0x4] : regs[0x2];
    c.src_stride_dw = imx51_g2d_regfile::StrideWords(cfg1);
    c.src_format    = src_format;
    c.foreground_color = regs[0x0A];
    // A one-bit mask source expands to alpha over white, including alpha zero.
    c.background_color = mask_source && src_format == 0u ? 0x00FFFFFFu : regs[0x0B];
    c.src_srgb      = (cfg1 & (1u << 17)) != 0u;
    c.src_swap_rb   = (cfg1 & (1u << 21)) != 0u;
    c.dst_srgb      = (regs[0xE] & (1u << 11)) != 0u;
    c.dst_swap_rb   = (cfg0 & (1u << 21)) != 0u;
    c.dither        = (regs[0xE] & (1u << 10)) != 0u;
    if (c.dither && (!CopyDest16(dst_format) ||
                     (src_format != kG2d8888 && src_format != kG2d8888Rgba && !CopyDest16(src_format))))
        Halt(regs, "dithered copy format (not modeled)", 0xEu, regs[0xE]);
    if (c.dither && CopyDest16(src_format) &&
        (blend || mask_source || c.src_srgb || c.dst_srgb || c.src_swap_rb || c.dst_swap_rb))
        Halt(regs, "dithered packed copy conversion (not modeled)", 0xEu, regs[0xE]);
    if (c.dst_srgb && blend)
        Halt(regs, "sRGB write with copy blending", 0xEu, regs[0xE]);
    c.premultiply_source = premultiply;
    c.opaque_source = opaque;
    c.unpremultiply_source = unpremultiply && CopyDest16(dst_format);
    const uint32_t gsx = regs[0x8], gsy = regs[0x9];  /* dest G2D scissor (inclusive) */
    c.clip_l = static_cast<int32_t>(gsx & 0x7FFu);
    c.clip_r = static_cast<int32_t>((gsx >> 11) & 0x7FFu);
    c.clip_t = static_cast<int32_t>(gsy & 0x7FFu);
    c.clip_b = static_cast<int32_t>((gsy >> 11) & 0x7FFu);
    c.dst_x = static_cast<int32_t>(xy << 4) >> 20;   /* G2D_XY.X[27:16] signed 12 */
    c.dst_y = static_cast<int32_t>(xy << 20) >> 20;  /* G2D_XY.Y[11:0] signed 12 */
    c.w     = static_cast<int32_t>(wh << 4) >> 20;   /* G2D_WIDTHHEIGHT.WIDTH[27:16] signed 12 */
    c.h     = static_cast<int32_t>(wh << 20) >> 20;  /* G2D_WIDTHHEIGHT.HEIGHT[11:0] signed 12 */
    if (c.dither && (c.w < 0 || c.h < 0) &&
        (!CopyDest16(c.src_format) || c.src_format != c.dst_format))
        Halt(regs, "reverse dithered copy (not modeled)", kWh, wh);
    c.src_x = static_cast<int32_t>((sxy >> 16) & 0x7FFu);  /* G2D_SXY.X[26:16] unsigned 11 */
    c.src_y = static_cast<int32_t>(sxy & 0x7FFu);          /* G2D_SXY.Y[10:0] unsigned 11 */
    CopyRect(c);
    if (unpremultiply && !c.unpremultiply_source) UnpremultiplyRect(c);
}

void Imx51Gpu2dDirect2d::CopyMask(const uint32_t (&regs)[0x100], uint32_t sxy) {
    using namespace imx51_g2d_blend;
    const bool mask_source = (regs[0xE] == 4u || regs[0xE] == 5u) && (regs[0xC] & 0x8000u);
    const uint32_t source_base = mask_source ? regs[4] : regs[2];
    const uint32_t cfg0 = regs[1], cfg1 = mask_source ? regs[5] : regs[3];
    const uint32_t format = (cfg1 >> 12) & 15u;
    const bool packed16 = Gpu2dTextureFormat16(format);
    const bool packed8 = Gpu2dTextureFormat8(format);
    const bool packed1 = Gpu2dTextureFormat1(format);
    const bool packed4 = Gpu2dTextureFormat4(format);
    const bool dest_blue = ((cfg0 >> 12) & 15u) == 3u;
    if ((format != kG2d8888 && format != kG2d8888Rgba && !packed16 && !packed8 && !packed1 && !packed4) ||
        (cfg1 & 0xFF5D0000u) || (cfg0 & 0xFF7F0000u) || !regs[0] || !source_base ||
        (mask_source && !(packed8 || packed4 || packed1)))
        Halt(regs, "mask copy surface config", 3u, cfg1);
    const bool blend = (regs[0x11] & 0x20u) != 0u;
    if ((regs[0xE] != 2u && regs[0xE] != 3u && !mask_source) ||
        (blend ? (regs[0x11] & ~0x13Fu) != 0u : (regs[0x11] & ~0x140u) != 0u) ||
        (regs[0xC] & ~(mask_source ? 0x8900u : 0x900u)) || regs[0xD] != 0x404u ||
        (regs[0xD0] & 0xC0u) || regs[0x10] != 0xFFFFFFu)
        Halt(regs, "mask copy blend config", 0x11u, regs[0x11]);
    const uint32_t alpha_passes = ((regs[0x11] >> 3) & 3u) + 1u;
    const uint32_t color_passes = (regs[0x11] & 7u) + 1u;
    for (uint32_t pass = 0; blend && pass < alpha_passes + color_passes; ++pass) {
        const uint32_t prog = pass < alpha_passes ? regs[0x14u + pass] : regs[0x18u + pass - alpha_passes];
        for (uint32_t i = 0; i < 4u; ++i)
            if (!Const(prog, i) && Src(prog, i) > kSrcTemp2)
                Halt(regs, "mask copy blend operand", 0x14u, prog);
    }
    const int32_t dst_stride = imx51_g2d_regfile::StrideWords(cfg0) * 4;
    const int32_t src_stride = imx51_g2d_regfile::StrideWords(cfg1) * 4;
    const uint32_t xy = regs[kXy], wh = regs[kWh];
    const int32_t x0 = static_cast<int32_t>(xy << 4) >> 20, y0 = static_cast<int32_t>(xy << 20) >> 20;
    const int32_t width = static_cast<int32_t>(wh << 4) >> 20, height = static_cast<int32_t>(wh << 20) >> 20;
    const int32_t sx = static_cast<int32_t>((sxy >> 16) & 0x7FFu), sy = static_cast<int32_t>(sxy & 0x7FFu);
    const int32_t left = static_cast<int32_t>(regs[8] & 0x7FFu), right = static_cast<int32_t>((regs[8] >> 11) & 0x7FFu);
    const int32_t top = static_cast<int32_t>(regs[9] & 0x7FFu), bottom = static_cast<int32_t>((regs[9] >> 11) & 0x7FFu);
    auto& mem = emu_.Get<EmulatedMemory>();
    const int32_t x_step = width < 0 ? -1 : 1, y_step = height < 0 ? -1 : 1;
    const int32_t columns = width * x_step + (height < 0 && width >= 0 ? 1 : 0);
    const int32_t rows = height * y_step;
    for (int32_t row = 0; row < rows; ++row) {
        const int32_t y = y0 + row * y_step;
        if (y < top || y > bottom) continue;
        for (int32_t col = 0; col < columns; ++col) {
            const int32_t x = x0 + col * x_step;
            if (x < left || x > right) continue;
            const uint32_t source_pa = source_base + static_cast<uint32_t>((sy + row * y_step) * src_stride + (packed1 ? (sx + col * x_step) / 8 : packed4 ? (sx + col * x_step) / 2 : (sx + col * x_step) * (packed8 ? 1 : packed16 ? 2 : 4)));
            const uint32_t dest_pa = regs[0] + static_cast<uint32_t>(y * dst_stride + x);
            const uint8_t* source = mem.TryTranslate(source_pa);
            uint8_t* dest = mem.TryTranslateWrite(dest_pa);
            if (!source || !dest) Halt(regs, "mask copy address unbacked", source_pa, dest_pa);
            uint32_t pixel;
            if (packed1) {
                const bool set = (*source & (1u << ((sx + col * x_step) % 8))) != 0u;
                pixel = format == 1u ? (set ? 0xFFFFFFFFu : 0xFF000000u)
                                     : regs[set ? 0x0Au : 0x0Bu];
                if (format == 1u && mask_source) pixel = set ? 0xFF000000u : 0u;
            } else if (packed4) {
                const uint32_t nibble = (*source >> (((sx + col * x_step) % 2) * 4)) & 15u;
                pixel = nibble * 17u * 0x01010101u;
            } else if (packed8) pixel = format == 3u && !mask_source
                ? 0xFF000000u | (uint32_t(*source) * 0x010101u) : uint32_t(*source) << 24;
            else if (packed16) pixel = Gpu2dDecodeTexture16(*reinterpret_cast<const uint16_t*>(source), format);
            else pixel = Imx51Gpu2dCopyColor(*reinterpret_cast<const uint32_t*>(source), format);
            if (cfg1 & (1u << 21))
                pixel = (pixel & 0xFF00FF00u) | ((pixel & 0x00FF0000u) >> 16) |
                        ((pixel & 0x000000FFu) << 16);
            const uint32_t destination = dest_blue ? 0xFF000000u | (uint32_t(*dest) * 0x010101u)
                                                   : uint32_t(*dest) << 24;
            const uint32_t out = blend ? AddProgram(regs + 0x14, alpha_passes, regs + 0x18, color_passes,
                                                    regs + 0xB0, pixel, destination) : pixel;
            *dest = static_cast<uint8_t>(dest_blue ? out : out >> 24);
        }
    }
}

/* Copy pixels to the ARGB8888 destination, clipped to the G2D scissor. */
void Imx51Gpu2dDirect2d::CopyRect(const Gpu2dCopySpec& c) {
    auto& mem = emu_.Get<EmulatedMemory>();
    const int32_t x_step = c.w < 0 ? -1 : 1;
    const int32_t y_step = c.h < 0 ? -1 : 1;
    // Reverse-Y commands encode a nonnegative X span as the last pixel offset.
    const int32_t width = c.w * x_step + (c.h < 0 && c.w >= 0 ? 1 : 0);
    const int32_t height = c.h * y_step;
    for (int32_t row = 0; row < height; ++row) {
        const int32_t dy = c.dst_y + row * y_step;
        if (dy < c.clip_t || dy > c.clip_b) continue;
        const int32_t sy = c.src_y + row * y_step;
        for (int32_t col = 0; col < width; ++col) {
            const int32_t dx = c.dst_x + col * x_step;
            if (dx < c.clip_l || dx > c.clip_r) continue;
            const int32_t sx = c.src_x + col * x_step;
            const uint32_t spa = c.src_pa + static_cast<uint32_t>(sy * c.src_stride_dw * 4 +
                (Gpu2dTextureFormat1(c.src_format) ? sx / 8 : Gpu2dTextureFormat4(c.src_format) ? sx / 2 :
                 Gpu2dTextureFormat8(c.src_format) ? sx :
                 Gpu2dTextureFormat16(c.src_format) ? sx * 2 : sx * 4));
            const uint8_t* shp = mem.TryTranslate(spa);
            if (!shp) {
                emu_.Get<Fatal>().Die("[GPU2D-D2D] copy source pixel unbacked pa=0x%08X (x=%d y=%d)",
                                      spa, sx, sy);
            }
            const uint32_t dpa = c.dst_pa + static_cast<uint32_t>(dy * c.dst_stride_dw * 4 +
                dx * (CopyDest16(c.dst_format) ? 2 : 4));
            uint8_t* dhp = mem.TryTranslateWrite(dpa);
            if (!dhp) {
                emu_.Get<Fatal>().Die("[GPU2D-D2D] copy dest pixel unbacked pa=0x%08X (x=%d y=%d)",
                                      dpa, dx, dy);
            }
            const bool bit_set = Gpu2dTextureFormat1(c.src_format) && (*shp & (1u << (sx % 8)));
            const uint32_t pixel = c.src_format == 0u ? (bit_set ? c.foreground_color : c.background_color)
                : c.src_format == 1u ? (bit_set ? 0xFFFFFFFFu : 0xFF000000u)
                : c.src_format == 2u ? (((uint32_t(*shp) >> ((sx % 2) * 4)) & 15u) * 17u << 24) | 0xFFFFFFu
                : c.src_format == 3u ? 0xFF000000u | (uint32_t(*shp) * 0x010101u)
                : c.src_format == 14u ? (uint32_t(*shp) << 24) | 0xFFFFFFu
                : Gpu2dTextureFormat16(c.src_format) ? Gpu2dDecodeTexture16(*reinterpret_cast<const uint16_t*>(shp), c.src_format)
                : *reinterpret_cast<const uint32_t*>(shp);
            uint32_t converted = Imx51Gpu2dCopyColor(pixel, c.src_format);
            if (c.src_swap_rb) converted = (converted & 0xFF00FF00u) |
                ((converted & 0x00FF0000u) >> 16) | ((converted & 0x000000FFu) << 16);
            if (c.src_srgb) converted = Imx51Gpu2dSrgbToLinear(converted);
            if (c.opaque_source) converted |= 0xFF000000u;
            if (c.premultiply_source) converted = imx51_g2d_blend::PremultArgb(converted);
            if (c.unpremultiply_source) converted = imx51_g2d_blend::UnpremultArgb(converted);
            if (c.dst_srgb) converted = Imx51Gpu2dLinearToSrgb(converted);
            if (c.dst_swap_rb) converted = (converted & 0xFF00FF00u) |
                ((converted >> 16) & 0xFFu) | ((converted & 0xFFu) << 16);
            if (c.dither) {
                const uint32_t x = static_cast<uint32_t>(col), y = static_cast<uint32_t>(row);
                converted = CopyDest16(c.src_format)
                    ? DitherFromPacked16(*reinterpret_cast<const uint16_t*>(shp), c.src_format, c.dst_format, x, y)
                    : DitherPacked16(converted, c.dst_format, x, y);
            }
            // A premultiplied color with quantized zero alpha cannot retain RGB.
            // Quantize first so dither can still select an opaque alpha sample.
            if (c.premultiply_source && (c.dst_format == 5u || c.dst_format == 12u) &&
                (converted >> 24) < 128u) converted = 0u;
            if (c.dst_format == kG2d0565) *reinterpret_cast<uint16_t*>(dhp) = lcd_pixel::PackRgb565(converted);
            else if (c.dst_format == 11u) {
                *reinterpret_cast<uint16_t*>(dhp) = static_cast<uint16_t>(((converted >> 20) & 15u) << 12 |
                    ((converted >> 12) & 15u) << 8 | ((converted >> 4) & 15u) << 4 | (converted >> 28));
            } else if (c.dst_format == 12u) {
                *reinterpret_cast<uint16_t*>(dhp) = static_cast<uint16_t>(((converted >> 19) & 31u) << 11 |
                    ((converted >> 11) & 31u) << 6 | ((converted >> 3) & 31u) << 1 | (converted >> 31));
            } else if (c.dst_format == 4u) {
                *reinterpret_cast<uint16_t*>(dhp) = static_cast<uint16_t>(((converted >> 28) & 15u) << 12 |
                    ((converted >> 20) & 15u) << 8 | ((converted >> 12) & 15u) << 4 | ((converted >> 4) & 15u));
            } else if (c.dst_format == 5u) {
                *reinterpret_cast<uint16_t*>(dhp) = static_cast<uint16_t>((converted >> 31) << 15 |
                    ((converted >> 19) & 31u) << 10 | ((converted >> 11) & 31u) << 5 | ((converted >> 3) & 31u));
            } else *reinterpret_cast<uint32_t*>(dhp) = c.dst_format == kG2d8888Rgba
                ? (converted << 8) | (converted >> 24) : converted;
        }
    }
}

void Imx51Gpu2dDirect2d::UnpremultiplyRect(const Gpu2dCopySpec& c) {
    auto& mem = emu_.Get<EmulatedMemory>();
    const int32_t x_step = c.w < 0 ? -1 : 1, y_step = c.h < 0 ? -1 : 1;
    const int32_t width = c.w * x_step + (c.h < 0 && c.w >= 0 ? 1 : 0);
    const int32_t height = c.h * y_step;
    for (int32_t row = 0; row < height; ++row) {
        const int32_t dy = c.dst_y + row * y_step;
        if (dy < c.clip_t || dy > c.clip_b) continue;
        for (int32_t col = 0; col < width; ++col) {
            const int32_t dx = c.dst_x + col * x_step;
            if (dx < c.clip_l || dx > c.clip_r) continue;
            const uint32_t dpa = c.dst_pa + static_cast<uint32_t>((dy * c.dst_stride_dw
                                                                   + dx) * 4);
            uint8_t* dhp = mem.TryTranslateWrite(dpa);
            if (!dhp) {
                emu_.Get<Fatal>().Die("[GPU2D-D2D] un-premultiply dest pixel unbacked pa=0x%08X (x=%d y=%d)",
                                      dpa, dx, dy);
            }
            auto* stored = reinterpret_cast<uint32_t*>(dhp);
            const uint32_t pixel = Imx51Gpu2dCopyColor(*stored, c.dst_format);
            const uint32_t alpha = pixel >> 24;
            if (alpha == 0u) { *stored = 0u; continue; }
            if (alpha == 255u) continue;
            using imx51_g2d_blend::UnpremultChannel;
            const uint32_t unpacked = (alpha << 24) | (UnpremultChannel((pixel >> 16) & 0xFFu, alpha) << 16)
                | (UnpremultChannel((pixel >> 8) & 0xFFu, alpha) << 8)
                | UnpremultChannel(pixel & 0xFFu, alpha);
            *stored = c.dst_format == kG2d8888Rgba ? (unpacked << 8) | (unpacked >> 24) : unpacked;
        }
    }
}

}

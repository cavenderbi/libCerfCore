#define NOMINMAX
#include "imx51_gpu2d_image_paint.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "imx51_id.h"
#include <cerfcore/cpu/emulated_memory.h>
#include "imx51_gpu2d_blend.h"
#include "imx51_gpu2d_regfile.h"
#include "imx51_gpu2d_gradw_sampler.h"

#include <algorithm>
#include <cmath>

namespace cerfcore {

namespace {
using namespace imx51_g2d_blend;
using namespace imx51_g2d_regfile;

constexpr uint32_t kBase0 = 0x00u, kCfg0 = 0x01u, kScisX = 0x08u, kScisY = 0x09u;
constexpr uint32_t kAlphaBlend = 0x0Cu, kRop = 0x0Du, kConfig = 0x0Eu, kInput = 0x0Fu;
constexpr uint32_t kMask = 0x10u, kBcfg = 0x11u;
constexpr uint32_t kBlendA0 = 0x14u, kBlendC0 = 0x18u;
constexpr uint32_t kConst0 = 0xC0u;                       /* CONST0-5 = 0xC0-0xC5 (affine coeffs) */
constexpr uint32_t kGradient = 0xD0u, kTexCfg = 0xD1u, kTexSize = 0xD2u, kTexBase = 0xD3u;
constexpr uint32_t kInst0 = 0xE0u, kInst1 = 0xE1u, kXy = 0xF0u, kWh = 0xF1u, kColor = 0xFFu;

/* Image-paint program 2 uses a two-instruction affine texture fetch. */
constexpr uint32_t kGradientImg = 0x188u;
constexpr uint32_t kInst0Affine = 0x10080632u, kInst1Affine = 0x12098695u;

}  // namespace

REGISTER_SERVICE(Imx51Gpu2dImagePaint);

bool Imx51Gpu2dImagePaint::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetSocId() == SocId::Imx51;
}

void Imx51Gpu2dImagePaint::Halt(const uint32_t (&regs)[0x100], const char* why,
                                uint32_t addr, uint32_t data) const {
    (void)regs;
    emu_.Get<Fatal>().Die("[GPU2D-IMGPAINT] %s at 0x%08X (value 0x%08X)", why, addr, data);
}

void Imx51Gpu2dImagePaint::CheckGates(const uint32_t (&regs)[0x100]) const {
    if (regs[kInput] != 0x09u)
        Halt(regs, "INPUT (not COLOR|COPYCOORD)", kInput, regs[kInput]);
    if ((regs[kGradient] & ~7u) != kGradientImg)  /* low 3 bits = INSTRUCTIONS (program 1, ENABLE[6] clear) */
        Halt(regs, "GRADIENT program (not the 2-instruction image fetch)", kGradient, regs[kGradient]);
    if (regs[kInst0] != kInst0Affine || regs[kInst1] != kInst1Affine)
        Halt(regs, "GRADW INST (not the S/T affine MAC)", kInst0, regs[kInst0]);
    const uint32_t texcfg = regs[kTexCfg];
    const uint32_t format = (texcfg >> 12) & 15u;
    if ((format != 7u && format != 13u && !Gpu2dTextureFormat16(format) && !Gpu2dTextureFormat8(format) && !Gpu2dTextureFormat1(format) && !Gpu2dTextureFormat4(format)) ||
        (texcfg & (0x2F1F0000u & ~(Gpu2dTextureFormat4(format) ? 0x04000000u : 0u))) ||
        (Gpu2dTextureFormat4(format) && !(texcfg & 0x04000000u)))
        Halt(regs, "image paint texture config", kTexCfg, texcfg);
    if ((regs[kTexSize] & 0x7FFu) == 0u || ((regs[kTexSize] >> 11) & 0x7FFu) == 0u)
        Halt(regs, "texture zero size", kTexSize, regs[kTexSize]);
    if (regs[kConfig] != 0x01u && regs[kConfig] != 0x05u)  /* DST[0]=1 only; SRC1=0 disables the second texture */
        Halt(regs, "CONFIG (not DST-only, SRC1 disabled)", kConfig, regs[kConfig]);
    const uint32_t bcfg = regs[kBcfg];
    if (!(bcfg & 0x20u) || (bcfg & 0x180u))
        Halt(regs, "image paint blender config", kBcfg, bcfg);
    if (regs[kAlphaBlend] & ~0x4100u)
        Halt(regs, "image paint alpha blend", kAlphaBlend, regs[kAlphaBlend]);
    const uint32_t alpha_passes = ((bcfg >> 3) & 3u) + 1u;
    const uint32_t color_passes = (bcfg & 7u) + 1u;
    for (uint32_t pass = 0; pass < alpha_passes + color_passes; ++pass) {
        const uint32_t prog = pass < alpha_passes
            ? regs[kBlendA0 + pass] : regs[kBlendC0 + pass - alpha_passes];
        for (uint32_t operand = 0; operand < 4u; ++operand)
            if (!Const(prog, operand) && Src(prog, operand) > kSrcTemp2)
                Halt(regs, "image paint blend operand", kBcfg, prog);
    }
    if (regs[kRop] != 0x404u)
        Halt(regs, "ROP", kRop, regs[kRop]);
    if (regs[kMask] != 0xFFFFFFu)
        Halt(regs, "MASK wrap", kMask, regs[kMask]);
    const uint32_t cfg0 = regs[kCfg0];
    if ((((cfg0 >> 12) & 0xFu) != 7u && !Gpu2dTextureFormat16((cfg0 >> 12) & 0xFu)) ||
        (cfg0 & 0xFF7F0000u))
        Halt(regs, "dest format (not linear G2D_8888)", kCfg0, cfg0);
    if (regs[kBase0] == 0u)
        Halt(regs, "dest BASE0 null bind", kBase0, 0);
    if (regs[kTexBase] == 0u)
        Halt(regs, "texture TEXBASE null bind", kTexBase, 0);
}

uint32_t Imx51Gpu2dImagePaint::BlendMultiPass(const uint32_t (&regs)[0x100],
                                              uint32_t src, uint32_t image, uint32_t dst, uint32_t coverage) const {
    /* PREMULTIPLYDST converts a straight-alpha destination for the blend ALU. */
    const uint32_t dstop = (regs[kAlphaBlend] & 0x4000u) ? PremultArgb(dst) : dst;
    const uint32_t passes = (regs[kBcfg] & 7u) + 1u;           /* PASSES[2:0] + 1 color passes */
    const uint32_t apasses = ((regs[kBcfg] >> 3) & 3u) + 1u;   /* ALPHAPASSES[4:3] + 1 alpha passes */
    uint32_t out = AddProgram(regs + kBlendA0, apasses, regs + kBlendC0, passes,
                              regs + 0xB0, src, dstop, image);
    if (coverage != 255u) {
        const uint32_t coverage_dst = (regs[kBcfg] & (1u << 6)) && !(regs[kAlphaBlend] & 0x4000u)
            ? PremultArgb(dst) : dstop;
        uint32_t mixed = 0u;
        for (uint32_t shift = 0; shift < 32u; shift += 8u) {
            const uint32_t result_channel = (out >> shift) & 255u;
            const uint32_t dest_channel = (coverage_dst >> shift) & 255u;
            mixed |= ((result_channel * coverage + dest_channel * (255u - coverage) + 127u) / 255u) << shift;
        }
        out = mixed;
    }
    if (regs[kBcfg] & (1u << 6)) {  /* OOALPHA: un-premultiply back to straight-alpha */
        const uint32_t a = out >> 24;
        if (a == 0u) {
            out = 0u;
        } else if (a != 255u) {
            out = (a << 24) | (UnpremultChannel((out >> 16) & 0xFFu, a) << 16)
                | (UnpremultChannel((out >> 8) & 0xFFu, a) << 8) | UnpremultChannel(out & 0xFFu, a);
        }
    }
    return out;
}

void Imx51Gpu2dImagePaint::CheckNormalGates(const uint32_t (&regs)[0x100]) const {
    if (regs[kInput] != 0x08u || ((regs[kGradient] & ~7u) != 0x40u && (regs[kGradient] & 0x1C0u) != 0x1C0u))
        Halt(regs, "normal image mode", kInput, regs[kInput]);

    const uint32_t texcfg = regs[kTexCfg];
    const uint32_t format = (texcfg >> 12) & 15u;
    if ((format != 7u && format != 13u && !Gpu2dTextureFormat16(format) && !Gpu2dTextureFormat8(format) && !Gpu2dTextureFormat1(format) && !Gpu2dTextureFormat4(format)) ||
        (texcfg & (0x271F0000u & ~(Gpu2dTextureFormat4(format) ? 0x04000000u : 0u))) ||
        (Gpu2dTextureFormat4(format) && !(texcfg & 0x04000000u)))
        Halt(regs, "normal image texture config", kTexCfg, texcfg);
    if ((regs[kTexSize] & 0x7FFu) == 0u || ((regs[kTexSize] >> 11) & 0x7FFu) == 0u ||
        regs[kTexBase] == 0u)
        Halt(regs, "normal image texture binding", kTexSize, regs[kTexSize]);

    if ((regs[kConfig] != 1u && regs[kConfig] != 5u) || !(regs[kBcfg] & 0x20u) || (regs[kBcfg] & 0x180u))
        Halt(regs, "normal image blender config", kBcfg, regs[kBcfg]);
    if ((regs[kAlphaBlend] & ~0x4100u) || regs[kRop] != 0x404u || regs[kMask] != 0xFFFFFFu)
        Halt(regs, "normal image blending state", kAlphaBlend, regs[kAlphaBlend]);
    if (((((regs[kCfg0] >> 12) & 15u) != 7u) && !Gpu2dTextureFormat16((regs[kCfg0] >> 12) & 15u)) ||
        (regs[kCfg0] & 0xFF7F0000u) || regs[kBase0] == 0u)
        Halt(regs, "normal image destination", kCfg0, regs[kCfg0]);

    const uint32_t alpha_passes = ((regs[kBcfg] >> 3) & 3u) + 1u;
    const uint32_t color_passes = (regs[kBcfg] & 7u) + 1u;
    for (uint32_t pass = 0; pass < alpha_passes + color_passes; ++pass) {
        const uint32_t prog = pass < alpha_passes
            ? regs[kBlendA0 + pass] : regs[kBlendC0 + pass - alpha_passes];

        for (uint32_t operand = 0; operand < 4u; ++operand)
            if (!Const(prog, operand) && Src(prog, operand) > kSrcTemp2)
                Halt(regs, "normal image blend operand", kBcfg, prog);
    }
}

void Imx51Gpu2dImagePaint::Composite(const uint32_t (&regs)[0x100], const uint32_t* paint_regs) {
    const bool dual = regs[kInput] == 0x08u && (regs[kGradient] & 0x1C0u) == 0x1C0u;
    if (dual && !paint_regs) Halt(regs, "missing paint texture", kGradient, regs[kGradient]);
    const bool normal_image = regs[kInput] == 0x08u && ((regs[kGradient] & ~7u) == 0x40u || dual);
    if (normal_image) CheckNormalGates(regs);
    else CheckGates(regs);
    const uint32_t xy = regs[kXy], wh = regs[kWh];
    const int32_t x0 = static_cast<int32_t>(xy << 4) >> 20;   /* G2D_XY.X[27:16] signed 12 */
    const int32_t y0 = static_cast<int32_t>(xy << 20) >> 20;  /* G2D_XY.Y[11:0] signed 12 */
    const int32_t w  = static_cast<int32_t>(wh << 4) >> 20;   /* WIDTH[27:16] signed 12 */
    const int32_t h  = static_cast<int32_t>(wh << 20) >> 20;  /* HEIGHT[11:0] signed 12 */
    if (w <= 0 || h <= 0) return;
    const bool masked = regs[kConfig] == 5u;
    const uint32_t mask_cfg = regs[0x05];
    if (masked && (((mask_cfg >> 12) & 15u) != 3u || (mask_cfg & 0xFF7F0000u) || !regs[0x04]))
        Halt(regs, "image mask source config", 0x05u, mask_cfg);
    const int32_t mask_stride = StrideWords(mask_cfg) * 4;
    const int32_t stride = StrideWords(regs[kCfg0]);
    const uint32_t dest_format = (regs[kCfg0] >> 12) & 15u;
    const bool packed_dest = Gpu2dTextureFormat16(dest_format);
    const uint32_t gsx = regs[kScisX], gsy = regs[kScisY];   /* LEFT[10:0]/RIGHT[21:11] inclusive */
    const int32_t cl = static_cast<int32_t>(gsx & 0x7FFu);
    const int32_t cr = static_cast<int32_t>((gsx >> 11) & 0x7FFu);
    const int32_t ct = static_cast<int32_t>(gsy & 0x7FFu);
    const int32_t cb = static_cast<int32_t>((gsy >> 11) & 0x7FFu);
    Gpu2dGradwPaint pg{};
    pg.texbase = regs[kTexBase];
    pg.foreground_color = regs[0x0A];
    pg.background_color = regs[0x0B];
    pg.srgb = (regs[0xD1] & (1u << 22)) != 0u;
    pg.swap_rb = (regs[0xD1] & (1u << 27)) != 0u;
    pg.texw    = static_cast<int32_t>(regs[kTexSize] & 0x7FFu);         /* WIDTH[10:0] */
    pg.texh    = static_cast<int32_t>((regs[kTexSize] >> 11) & 0x7FFu); /* HEIGHT[21:11] */
    pg.format  = (regs[kTexCfg] >> 12) & 15u;
    pg.tstride = (regs[kTexCfg] & 0xFFFu) + 1u;                         /* TEXCFG.STRIDE + 1 */
    pg.bilinear    = (regs[kTexCfg] & (1u << 21)) != 0u;               /* TEXCFG.BILIN (gated 0) */
    const bool premultiply_texel = (regs[kTexCfg] & (1u << 23)) != 0u;
    pg.premultiply = !premultiply_texel;
    pg.ninst = (normal_image && !dual ? (regs[kGradient] & 7u) : ((regs[kGradient] >> 3) & 7u)) + 1u;
    for (uint32_t i = 0; i < pg.ninst && i < 8u; ++i) pg.inst[i] = regs[kInst0 + i];
    for (uint32_t i = 0; i < 12u; ++i) pg.cnst[i] = UnpackGradwConst(regs[kConst0 + i]);
    Gpu2dGradwPaint paint{};
    if (dual) {
        const uint32_t cfg = paint_regs[kTexCfg];
        const uint32_t format = (cfg >> 12) & 15u;
        if ((format != 7u && format != 13u && !Gpu2dTextureFormat16(format) &&
             !Gpu2dTextureFormat8(format) && !Gpu2dTextureFormat1(format) &&
             !Gpu2dTextureFormat4(format)) ||
            (cfg & (0x27010000u & ~(Gpu2dTextureFormat4(format) ? 0x04000000u : 0u))) ||
            (Gpu2dTextureFormat4(format) && !(cfg & 0x04000000u)) ||
            !paint_regs[kTexBase] || !(paint_regs[kTexSize] & 0x7FFu) ||
            !((paint_regs[kTexSize] >> 11) & 0x7FFu))
            Halt(regs, "paint texture binding", kTexCfg, cfg);
        paint.texbase = paint_regs[kTexBase];
        paint.texw = static_cast<int32_t>(paint_regs[kTexSize] & 0x7FFu);
        paint.texh = static_cast<int32_t>((paint_regs[kTexSize] >> 11) & 0x7FFu);
        paint.format = (cfg >> 12) & 15u;
        paint.tstride = (cfg & 0xFFFu) + 1u;
        paint.bilinear = (cfg & (1u << 21)) != 0u;
        paint.srgb = (cfg & (1u << 22)) != 0u;
        paint.swap_rb = (cfg & (1u << 27)) != 0u;
        paint.premultiply = (cfg & (1u << 23)) == 0u;
        paint.wrap_u = static_cast<Gpu2dWrap>((cfg >> 17) & 3u);
        paint.wrap_v = static_cast<Gpu2dWrap>((cfg >> 19) & 3u);
        paint.border_color = paint_regs[0xD4];
        paint.foreground_color = paint_regs[0x0A];
        paint.background_color = paint_regs[0x0B];
        paint.ninst = (paint_regs[kGradient] & 7u) + 1u;
        for (uint32_t i = 0; i < paint.ninst && i < 8u; ++i) paint.inst[i] = paint_regs[kInst0 + i];
        for (uint32_t i = 0; i < 12u; ++i) paint.cnst[i] = UnpackGradwConst(paint_regs[kConst0 + i]);
    }
    // SOURCE replacement into a straight-alpha surface cancels premultiplication.
    // Keep the original texel until after that cancellation to avoid 8-bit loss.
    const bool straight_source_copy = normal_image && !dual && !masked &&
        !pg.premultiply && !pg.bilinear && regs[kBcfg] == 0x60u &&
        regs[kBlendA0] == 0x00812004u && regs[kBlendC0] == 0x00812004u;
    auto& samp = emu_.Get<Imx51Gpu2dGradwSampler>();
    auto& mem = emu_.Get<EmulatedMemory>();
    for (int32_t row = 0; row < h; ++row) {
        const int32_t py = y0 + row;
        if (py < ct || py > cb) continue;
        for (int32_t col = 0; col < w; ++col) {
            const int32_t px = x0 + col;
            if (px < cl || px > cr) continue;
            const uint32_t image = straight_source_copy
                ? samp.SampleTexel(pg, px, py) : samp.Sample(pg, px, py);
            const uint32_t dpa = regs[kBase0] + static_cast<uint32_t>(int64_t(py) * stride * 4 +
                                                 int64_t(px) * (packed_dest ? 2 : 4));
            uint8_t* dhp = mem.TryTranslateWrite(dpa);
            if (!dhp) {
                emu_.Get<Fatal>().Die("[GPU2D-IMGPAINT] dest unbacked pa=0x%08X (x=%d y=%d)",
                                      dpa, px, py);
            }
            const uint32_t destination = packed_dest
                ? Gpu2dDecodeTexture16(*reinterpret_cast<const uint16_t*>(dhp), dest_format)
                : *reinterpret_cast<const uint32_t*>(dhp);
            uint32_t coverage = 255u;
            if (masked) {
                const uint32_t mask_pa = regs[0x04] + static_cast<uint32_t>(py * mask_stride + px);
                const uint8_t* mask = mem.TryTranslate(mask_pa);
                if (!mask) Halt(regs, "image mask unbacked", 0x04u, mask_pa);
                coverage = *mask;
            }
            uint32_t result = straight_source_copy ? ((image >> 24) ? image : 0u)
                : dual ? BlendMultiPass(regs, samp.Sample(paint, px, py), image, destination, coverage)
                : normal_image ? BlendMultiPass(regs, image, 0xFFFFFFFFu, destination, coverage)
                : BlendMultiPass(regs, regs[kColor], image, destination, coverage);
            // A premultiplied one-bit-alpha output cannot retain color when alpha quantizes to zero.
            if ((dest_format == 5u || dest_format == 12u) &&
                !(regs[kBcfg] & (1u << 6)) && (result >> 24) < 128u) {
                result = 0u;
            }
            if (packed_dest) *reinterpret_cast<uint16_t*>(dhp) = Gpu2dEncodeTexture16(result, dest_format);
            else *reinterpret_cast<uint32_t*>(dhp) = result;
        }
    }
}

}

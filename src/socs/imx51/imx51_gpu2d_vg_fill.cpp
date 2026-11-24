#define NOMINMAX
#include "imx51_gpu2d_vg_fill.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "imx51_id.h"
#include "imx51_gpu2d_rasterizer.h"
#include "imx51_gpu2d_stroker.h"
#include "imx51_gpu2d_regfile.h"
#include "imx51_gpu2d_gradw_sampler.h"
#include "imx51_gpu2d_blend.h"

#include <algorithm>

namespace cerfcore {

namespace {
using namespace imx51_g2d_regfile;
constexpr uint32_t kMode  = 0x6Eu;  /* VGV2_MODE: STROKE[8], CAP[5:4], JOIN[7:6], EXPONENTADD[23:18] */
constexpr uint32_t kXf    = 0x50u;  /* VGV2 XF affine XFXX..XFYA = 0x50-0x55 */
constexpr uint32_t kMiter = 0x66u;  /* VGV2 MITER = -cos(2*asin(1/miterLimit)) (sub_41C5B1B8) */
}  // namespace

REGISTER_SERVICE(Imx51Gpu2dVgFill);

bool Imx51Gpu2dVgFill::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetSocId() == SocId::Imx51;
}

void Imx51Gpu2dVgFill::Halt(const uint32_t (&regs)[0x100], const char* why,
                            uint32_t addr, uint32_t data) const {
    (void)regs;
    emu_.Get<Fatal>().Die("[GPU2D-VGFILL] %s at 0x%08X (value 0x%08X)", why, addr, data);
}

/* Every un-modeled paint/blend/dest mode halts (FATAL-first). */
void Imx51Gpu2dVgFill::Flush(const uint32_t (&regs)[0x100], bool bbox_live, const uint32_t* paint_regs) {
    auto& rast = emu_.Get<Imx51Gpu2dRasterizer>();
    if (!rast.HasGeometry()) return;
    const bool stroke = (regs[kMode] & (1u << 8)) != 0u;  /* VGV2_MODE.STROKE */
    /* G2D_GRADIENT bits (vgregs_z160.h:780-786): INSTRUCTIONS[2:0], ENABLE[6],
       ENABLE2[7], SEL[8]. ENABLE-only, count masked out = 0x40 = the gradient program. */
    const bool dual = (regs[0xD0] & 0x1C0u) == 0x1C0u;
    const bool image_paint = (regs[0xD0] & ~7u) == 0x188u;
    const bool gradw_grad = (regs[0xD0] & ~7u) == 0x40u || image_paint || dual;
    if (dual && !paint_regs) Halt(regs, "missing paint texture", 0xD0u, regs[0xD0]);
    if (regs[0xD0] & 0xC0u) {
        if (!gradw_grad)
            Halt(regs, "GRADW ENABLE2 paint (image-paint composite not in VG-fill)",
                 0xD0u, regs[0xD0]);
        /* INST0..INST(INSTRUCTIONS) run through the shared GRADW ISA evaluator, which
           halts on any un-modeled opcode/GRADREG - so no exact INST-word gate here. */
        /* TEXCFG (REG_GRADW_TEXCFG): FORMAT[15:12] must be 7 (G2D_8888); BILIN[21] /
           PREMULTIPLY[23] / TEX2D[28] and WRAPU/V[17:20] (CLAMP=0/REPEAT=1) are handled by
           the sampler. TILED[16] / SWAP*[24:27,29] stay unmodeled -> must be 0. */
        const uint32_t tc = regs[0xD1];
        const uint32_t format = (tc >> 12) & 0xFu;
        if ((format != 7u && format != 13u && !Gpu2dTextureFormat16(format) && !Gpu2dTextureFormat8(format) && !Gpu2dTextureFormat1(format) && !Gpu2dTextureFormat4(format)) ||
        (tc & (0x27010000u & ~(Gpu2dTextureFormat4(format) ? 0x04000000u : 0u))) ||
        (Gpu2dTextureFormat4(format) && !(tc & 0x04000000u)))
            Halt(regs, "GRADW texture config (format/tiled/swap)", 0xD1u, tc);
        if ((regs[0xD2] & 0x7FFu) == 0u || ((regs[0xD2] >> 11) & 0x7FFu) == 0u)
            Halt(regs, "GRADW texture zero size", 0xD2u, regs[0xD2]);
        if (regs[0xD3] == 0u)
            Halt(regs, "GRADW TEXBASE null bind", 0xD3u, 0);
    }
    const uint32_t bcfg = regs[0x11];          /* G2D_BLENDERCFG (vgregs_z160.h:356-363) */
    /* sync_2 libOpenVG.dll 0x41C58230 / 0x41C60610: TEMP1=SOURCE*CONST0+CONST1,
       then TEMP0=TEMP1+DEST*(1-TEMP1.a). City Center keyboard color transform. */
    const bool color_transform = (bcfg & 0x1Fu) == 9u &&
        regs[0x14] == 0x60418008u && regs[0x18] == 0x60418008u &&
        regs[0x15] == 0x0A85A004u && regs[0x19] == 0x0A85A804u;
    const bool generic_blend = dual || image_paint || (gradw_grad && (bcfg & 0x1Fu) && !color_transform);
    if (bcfg & (1u << 5)) {                    /* ENABLE */
        if ((bcfg & 0x1Fu) && !color_transform && !generic_blend) /* PASSES[2:0] / ALPHAPASSES[4:3] */
            Halt(regs, "G2D multi-pass blend (not modeled)", 0x11u, bcfg);
        if (bcfg & 0x180u)                     /* OBS_DIVALPHA[7] / NOMASK[8] */
            Halt(regs, "G2D blender DIVALPHA/NOMASK (not modeled)", 0x11u, bcfg);
        /* OBS_ENABLE[8] ambient; PREMULTIPLYDST[14] modeled in BlendPixel. */
        if (regs[0xC] & ~0x4100u)
            Halt(regs, "G2D ALPHABLEND modulation (not modeled)", 0xCu, regs[0xC]);
    }
    /* G2D_ROP: 0 = power-on, 0x404 = the surface-init ambient (sub_41C61DB4). */
    if (regs[0xD] != 0u && regs[0xD] != 0x404u)
        Halt(regs, "G2D ROP (not modeled)", 0xDu, regs[0xD]);
    if (regs[0x10] != 0xFFFFFFu)               /* G2D_MASK identity wrap */
        Halt(regs, "G2D_MASK non-identity wrap (not modeled)", 0x10u, regs[0x10]);
    /* VGV2 SCALE/BIAS = the path-data interpretation pair (context init writes 1.0/0.0). */
    if (regs[0x5E] != 0x3F800000u || regs[0x5F] != 0u)
        Halt(regs, "VGV2 path SCALE/BIAS non-identity (not modeled)", 0x5Eu, regs[0x5E]);
    /* VG solid fill = COLOR|VGMODE (0x11) or the COPYCOORD rider (0x19); the CONFIG
       gate below halts any source-fetch enable so COPYCOORD has nothing to route. */
    if (regs[0xF] != 0x11u && regs[0xF] != 0x19u)
        Halt(regs, "G2D INPUT mode (not modeled)", 0xFu, regs[0xF]);
    /* G2D_CONFIG: 0 / ARGBMASK-all (0xF000) flat fills; DST[0]=1 = the blend DEST read. */
    if (regs[0xE] != 0u && regs[0xE] != 0xF000u && regs[0xE] != 0x1u && regs[0xE] != 0x5u)
        Halt(regs, "G2D CONFIG mode (not modeled)", 0xEu, regs[0xE]);
    if ((regs[0x28] & 3u) > 2u)                /* VGV1_CFG2.AAMODE */
        Halt(regs, "VGV1 AAMODE (not modeled)", 0x28u, regs[0x28]);
    const uint32_t cfg0 = regs[0x1];
    const uint32_t dest_format = (cfg0 >> 12) & 0xFu;
    if (dest_format != 7u && dest_format != 14u && dest_format != 4u && dest_format != 5u &&
        dest_format != 6u && dest_format != 11u && dest_format != 12u)
        Halt(regs, "dest format (not modeled)", 0x1u, cfg0);
    if (cfg0 & 0xFF7F0000u)                    /* TILED[16]/SRGB[17]/SWAP fields;
                                                  STRIDESIGN[23] belongs to the stride */
        Halt(regs, "dest tiled/swapped/srgb (not modeled)", 0x1u, cfg0);
    if (regs[0x0] == 0u)                       /* null dest bind */
        Halt(regs, "dest BASE0 null bind", 0x0u, 0);
    Gpu2dFillTarget t{};
    t.dest_pa   = regs[0x0];
    t.dest_a8   = dest_format == 14u;
    t.dest_argb4444 = dest_format == 4u;
    t.dest_argb1555 = dest_format == 5u;
    t.dest_565 = dest_format == 6u;
    t.dest_4444 = dest_format == 11u;
    t.dest_5551 = dest_format == 12u;
    t.stride_dw = imx51_g2d_regfile::StrideWords(cfg0);
    /* sync_2 libOpenVG.dll 0x41C5B468 tile loop: VGV1_SCISSORX/Y (0x24/0x25) hold the
       tile clamped in a grid anchored at 0, VGV1_TILEOFS (0x22) that tile's grid origin
       plus the path-bbox sub-tile remainder, and G2D_SCISSORX/Y (0x8/0x9) the device
       scissor - so device = grid + (TILEOFS - VGV1 near edge). */
    const uint32_t gsx = regs[0x8], gsy = regs[0x9];
    const uint32_t bsx = regs[0x24], bsy = regs[0x25];
    const int32_t v_l = static_cast<int32_t>(bsx & 0x7FFu);
    const int32_t v_t = static_cast<int32_t>(bsy & 0x7FFu);
    const int32_t ox = static_cast<int32_t>(regs[0x22] & 0xFFFu) - v_l;
    const int32_t oy = static_cast<int32_t>((regs[0x22] >> 12) & 0xFFFu) - v_t;
    t.clip_l = std::max(static_cast<int32_t>(gsx & 0x7FFu), v_l + ox);
    t.clip_r = std::min(static_cast<int32_t>((gsx >> 11) & 0x7FFu),
                        static_cast<int32_t>((bsx >> 16) & 0x7FFu) + ox);
    t.multisample = (regs[0x28] & 3u) == 2u;
    t.sample_scissor_top = static_cast<int32_t>(gsy & 0x7FFu);
    t.sample_scissor_bottom = static_cast<int32_t>((gsy >> 11) & 0x7FFu);
    const int32_t scissor_top = t.multisample ? t.sample_scissor_top / 4 : t.sample_scissor_top;
    const int32_t scissor_bottom = t.multisample ? t.sample_scissor_bottom / 4 : t.sample_scissor_bottom;
    t.clip_t = std::max(scissor_top, v_t + oy);
    t.clip_b = std::min(scissor_bottom, static_cast<int32_t>((bsy >> 16) & 0x7FFu) + oy);
    /* The tile loops write BBOX as band/tile rect + guard margins folded into XFT, so
       (bbox+XFT)*2^exp is grid-space and takes the same ox/oy shift as the clip. */
    if (bbox_live) {
        const float ke = DeviceScale(regs);
        const float fx = static_cast<float>(ox), fy = static_cast<float>(oy);
        if ((RegF(regs, 0x5A) + RegF(regs, 0x54)) * ke + fx > static_cast<float>(t.clip_l) + 0.5f ||
            (RegF(regs, 0x5C) + RegF(regs, 0x54)) * ke + fx < static_cast<float>(t.clip_r) - 0.5f ||
            (RegF(regs, 0x5B) + RegF(regs, 0x55)) * ke + fy > static_cast<float>(t.clip_t) + 0.5f ||
            (RegF(regs, 0x5D) + RegF(regs, 0x55)) * ke + fy < static_cast<float>(t.clip_b) - 0.5f)
            Halt(regs, "VGV2 BBOX binds tighter than the scissor clip (not modeled)",
                 0x5Au, regs[0x5A]);
    }
    if (regs[0xE] == 0x5u) {
        const uint32_t cfg = regs[0x05];
        if (((cfg >> 12) & 15u) != 3u || (cfg & 0xFF7F0000u) || !regs[0x04] ||
            !(bcfg & (1u << 5)))
            Halt(regs, "G2D mask source config", 0x05u, cfg);
        t.mask_pa = regs[0x04];
        t.mask_stride_dw = StrideWords(cfg);
    }
    t.argb        = regs[0xFF];                /* G2D_COLOR, premultiplied ARGB8888 */
    t.antialias   = (regs[0x28] & 3u) != 0u;
    t.even_odd    = (regs[0x27] & 1u) != 0u;   /* VGV1_CFG1.WINDRULE */
    t.blend       = (bcfg & (1u << 5)) != 0u;
    t.oo_alpha    = (bcfg & (1u << 6)) != 0u;
    t.premult_dst = (regs[0xC] & 0x4000u) != 0u;  /* ALPHABLEND.PREMULTIPLYDST */
    t.prog_a      = regs[0x14];                /* G2D_BLEND_A0 */
    t.prog_c      = regs[0x18];                /* G2D_BLEND_C0 */
    t.image_paint = image_paint;
    t.generic_blend = generic_blend;
    if (generic_blend) {
        t.alpha_passes = ((bcfg >> 3) & 3u) + 1u;
        t.color_passes = (bcfg & 7u) + 1u;
        std::copy_n(regs + 0x14, 4, t.alpha_program);
        std::copy_n(regs + 0x18, 8, t.color_program);
        for (uint32_t pass = 0; pass < t.alpha_passes + t.color_passes; ++pass) {
            const uint32_t prog = pass < t.alpha_passes
                ? t.alpha_program[pass] : t.color_program[pass - t.alpha_passes];
            for (uint32_t operand = 0; operand < 4u; ++operand)
                if (!imx51_g2d_blend::Const(prog, operand) &&
                    imx51_g2d_blend::Src(prog, operand) > imx51_g2d_blend::kSrcTemp2)
                    Halt(regs, "G2D blend operand (not modeled)", 0x11u, prog);
        }
    }
    t.color_transform = color_transform;
    t.prog_a1 = regs[0x15];
    t.prog_c1 = regs[0x19];
    std::copy_n(regs + 0xB0, 8, t.blend_const);
    /* GRADW gradient paint: per-pixel SOURCE = the fmt7 ramp texel at the (OUTX,OUTY)
       the ISA program (INST0..INST(INSTRUCTIONS)) computes from CONST0-C11. */
    Gpu2dGradwPaint pg{};
    if (gradw_grad) {
        pg.texbase     = regs[0xD3];
        pg.foreground_color = regs[0x0A];
        pg.background_color = regs[0x0B];
        pg.srgb = (regs[0xD1] & (1u << 22)) != 0u;
        pg.swap_rb = (regs[0xD1] & (1u << 27)) != 0u;
        pg.texw        = static_cast<int32_t>(regs[0xD2] & 0x7FFu);
        pg.texh        = static_cast<int32_t>((regs[0xD2] >> 11) & 0x7FFu);
        pg.format      = (regs[0xD1] >> 12) & 15u;
        pg.tstride     = (regs[0xD1] & 0xFFFu) + 1u;
        pg.bilinear    = (regs[0xD1] & (1u << 21)) != 0u;  /* TEXCFG.BILIN */
        const bool premultiply_texel = (regs[0xD1] & (1u << 23)) != 0u;
        pg.premultiply = !premultiply_texel;  /* TEXCFG.PREMULTIPLY */
        pg.wrap_u      = static_cast<Gpu2dWrap>((regs[0xD1] >> 17) & 3u);
        pg.wrap_v      = static_cast<Gpu2dWrap>((regs[0xD1] >> 19) & 3u);
        pg.border_color = regs[0xD4];
        pg.ninst = ((image_paint || dual) ? ((regs[0xD0] >> 3) & 7u) : (regs[0xD0] & 7u)) + 1u;                 /* GRADIENT.INSTRUCTIONS[2:0] + 1 (program 1) */
        for (uint32_t i = 0; i < pg.ninst && i < 8u; ++i) pg.inst[i] = regs[0xE0u + i];
        for (uint32_t i = 0; i < 12u; ++i) pg.cnst[i] = UnpackGradwConst(regs[0xC0u + i]);
        t.paint = &pg;
    }
    Gpu2dGradwPaint paint{};
    if (dual) {
        const uint32_t cfg = paint_regs[0xD1];
        const uint32_t format = (cfg >> 12) & 15u;
        if ((format != 7u && format != 13u && !Gpu2dTextureFormat16(format) &&
             !Gpu2dTextureFormat8(format) && !Gpu2dTextureFormat1(format) &&
             !Gpu2dTextureFormat4(format)) ||
            (cfg & (0x27010000u & ~(Gpu2dTextureFormat4(format) ? 0x04000000u : 0u))) ||
            (Gpu2dTextureFormat4(format) && !(cfg & 0x04000000u)) ||
            !paint_regs[0xD3] || !(paint_regs[0xD2] & 0x7FFu) ||
            !((paint_regs[0xD2] >> 11) & 0x7FFu))
            Halt(regs, "paint texture binding", 0xD1u, cfg);
        paint.texbase = paint_regs[0xD3];
        paint.texw = static_cast<int32_t>(paint_regs[0xD2] & 0x7FFu);
        paint.texh = static_cast<int32_t>((paint_regs[0xD2] >> 11) & 0x7FFu);
        paint.format = format; paint.tstride = (cfg & 0xFFFu) + 1u;
        paint.bilinear = (cfg & (1u << 21)) != 0u;
        paint.srgb = (cfg & (1u << 22)) != 0u;
        paint.swap_rb = (cfg & (1u << 27)) != 0u;
        paint.premultiply = (cfg & (1u << 23)) == 0u;
        paint.wrap_u = static_cast<Gpu2dWrap>((cfg >> 17) & 3u);
        paint.wrap_v = static_cast<Gpu2dWrap>((cfg >> 19) & 3u);
        paint.border_color = paint_regs[0xD4];
        paint.foreground_color = paint_regs[0x0A]; paint.background_color = paint_regs[0x0B];
        paint.ninst = (paint_regs[0xD0] & 7u) + 1u;
        for (uint32_t i = 0; i < paint.ninst && i < 8u; ++i) paint.inst[i] = paint_regs[0xE0u + i];
        for (uint32_t i = 0; i < 12u; ++i) paint.cnst[i] = UnpackGradwConst(paint_regs[0xC0u + i]);
        t.second_paint = &paint;
    }
    if (!stroke) { rast.Flush(t); return; }
    /* VGV2_THINRADIUS: the stroke builder sub_41C5B1B8/setup sub_41C63970 never
       write it (only RADIUS/MITER/ARC/ACCURACY), so it stays 0; a non-zero value
       is a thin-stroke min-width clamp the stroker does not model. */
    if (regs[0x61] != 0u)
        Halt(regs, "VGV2 thin-stroke THINRADIUS clamp (not modeled)", 0x61u, regs[0x61]);
    /* Stroke: RADIUS (0x65) half-width + the XF affine + 2^EXPONENTADD + CAP/JOIN
       dilate the centerline (stroke builder sub_41C5B1B8). */
    emu_.Get<Imx51Gpu2dStroker>().Stroke(t, RegF(regs, 0x65), RegF(regs, kXf), RegF(regs, kXf + 2u),
                RegF(regs, kXf + 1u), RegF(regs, kXf + 3u), DeviceScale(regs),
                (regs[kMode] >> 4) & 3u, (regs[kMode] >> 6) & 3u, RegF(regs, kMiter));
}

}

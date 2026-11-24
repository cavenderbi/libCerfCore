#define NOMINMAX
#include "imx51_gpu2d_gradw_sampler.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "imx51_id.h"
#include <cerfcore/cpu/emulated_memory.h>
#include "imx51_gpu2d_blend.h"
#include "imx51_gpu2d_direct2d.h"

#include <algorithm>
#include <cmath>

namespace cerfcore {

namespace {
/* Per-channel linear blend of two ARGB8888 texels by frac in [0,1] (round). */
uint32_t Lerp(uint32_t a, uint32_t b, float f) {
    uint32_t out = 0;
    for (uint32_t sh = 0; sh < 32u; sh += 8u) {
        const float ca = static_cast<float>((a >> sh) & 0xFFu);
        const float cb = static_cast<float>((b >> sh) & 0xFFu);
        out |= (static_cast<uint32_t>(ca + (cb - ca) * f + 0.5f) & 0xFFu) << sh;
    }
    return out;
}
enum { kOpDot = 0, kOpRcp = 1, kOpSqrtMul = 2, kOpSqrtAdd = 3 };
/* REPEAT tiling (G2D_WRAP_REPEAT): wrap a texel coord into [0,n), n>=1. */
int32_t WrapRepeat(int32_t v, int32_t n) {
    v %= n;
    return v < 0 ? v + n : v;
}
int32_t WrapCoord(int32_t value, int32_t size, Gpu2dWrap wrap) {
    if (wrap == Gpu2dWrap::Repeat) return WrapRepeat(value, size);
    if (wrap == Gpu2dWrap::Mirror) {
        const int32_t mirrored = WrapRepeat(value, size * 2);
        return mirrored < size ? mirrored : size * 2 - mirrored - 1;
    }
    return std::clamp(value, 0, size - 1);
}
}  // namespace

REGISTER_SERVICE(Imx51Gpu2dGradwSampler);

bool Imx51Gpu2dGradwSampler::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetSocId() == SocId::Imx51;
}

/* GRADW ISA (vgenums_z160.h:188-193 G2D_GRAD_OP; vgregs_z160.h:928-937 INST
   SRC_E:5/SRC_D:5/SRC_C:5/SRC_B:5/SRC_A:5/DST:4/OPCODE:2). DOT = A*B+C*D+E;
   SQRTMUL = sqrt(A)*B+C*D+E - NOT sqrt(A*B): INST3 SQRTMUL(dist^2, C6=1/radius)
   must equal the OpenVG radial parameter dist/r, and C6=1/r forces the sqrt(A)*B form. */
void Imx51Gpu2dGradwSampler::EvalProgram(const Gpu2dGradwPaint& p, int32_t px, int32_t py,
                                         float& outx, float& outy) const {
    float r[32] = {0.0f};
    r[0] = static_cast<float>(px);   /* GRADREG_X */
    r[1] = static_cast<float>(py);   /* GRADREG_Y */
    for (uint32_t i = 0; i < 12u; ++i) r[16u + i] = p.cnst[i];  /* C0-C11 */
    r[29] = 1.0f;    /* ONE  (ZERO=r[28] already 0) */
    r[30] = -1.0f;   /* MINUSONE */

    auto val = [&](uint32_t idx) -> float {
        if (idx <= 9u || (idx >= 16u && idx <= 30u)) return r[idx];
        emu_.Get<Fatal>().Die("[GPU2D-GRADW] ISA reads un-modeled GRADREG %u", idx);
    };

    const uint32_t n = std::min(p.ninst, 8u);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t w   = p.inst[i];
        const float    ve  = val(w & 0x1Fu),        vd = val((w >> 5) & 0x1Fu);
        const float    vc  = val((w >> 10) & 0x1Fu), vb = val((w >> 15) & 0x1Fu);
        const float    va  = val((w >> 20) & 0x1Fu);
        const uint32_t dst = (w >> 25) & 0xFu, op = (w >> 29) & 0x3u;
        float res;
        if (op == kOpDot)          res = va * vb + vc * vd + ve;
        else if (op == kOpRcp)     res = vb / va + vc * vd + ve;
        else if (op == kOpSqrtMul) res = std::sqrt(va) * vb + vc * vd + ve;
        else if (op == kOpSqrtAdd && vb == 0.0f && vd == 0.0f && ve == 0.0f)
            res = std::sqrt(std::max(va, 0.0f)) + vc;
        else {
            emu_.Get<Fatal>().Die("[GPU2D-GRADW] ISA opcode %u not modeled (INST%u=0x%08X)", op, i, w);
        }
        // The projective reciprocal program emits both texture coordinates:
        // OUTX = B/A and OUTY = D/A. The guest uses ZERO for C and E.
        if (op == kOpRcp && dst == 8u && ((w >> 10) & 31u) == 28u && (w & 31u) == 28u)
            r[9] = vd / va;
        if (dst > 9u) {  /* writable: X/Y-as-temp, temps 2-7, OUTX(8)/OUTY(9) */
            emu_.Get<Fatal>().Die("[GPU2D-GRADW] ISA writes un-modeled GRADREG %u", dst);
        }
        r[dst] = res;
    }
    outx = r[8];   /* GRADREG_OUTX */
    outy = r[9];   /* GRADREG_OUTY */
}

uint32_t Imx51Gpu2dGradwSampler::Fetch(const Gpu2dGradwPaint& p, int32_t tx, int32_t ty) const {
    if ((p.wrap_u == Gpu2dWrap::Border && (tx < 0 || tx >= p.texw)) ||
        (p.wrap_v == Gpu2dWrap::Border && (ty < 0 || ty >= p.texh)))
        return p.srgb ? Imx51Gpu2dSrgbToLinear(p.border_color) : p.border_color;
    tx = WrapCoord(tx, p.texw, p.wrap_u);
    ty = WrapCoord(ty, p.texh, p.wrap_v);
    const bool packed16 = Gpu2dTextureFormat16(p.format);
    const bool packed8 = Gpu2dTextureFormat8(p.format);
    const bool packed1 = Gpu2dTextureFormat1(p.format);
    const bool packed4 = Gpu2dTextureFormat4(p.format);
    uint32_t bytes_per_pixel = 4u;
    if (packed8) bytes_per_pixel = 1u;
    else if (packed16) bytes_per_pixel = 2u;
    const uint32_t pa = p.texbase + static_cast<uint32_t>(ty) * p.tstride * 4u
        + (packed1 ? static_cast<uint32_t>(tx) / 8u : packed4 ? static_cast<uint32_t>(tx) / 2u : static_cast<uint32_t>(tx) * bytes_per_pixel);
    const uint8_t* hp = emu_.Get<EmulatedMemory>().TryTranslate(pa);
    if (!hp) {
        emu_.Get<Fatal>().Die("[GPU2D-GRADW] texel unbacked pa=0x%08X (tx=%d ty=%d)", pa, tx, ty);
    }
    if (packed1) {
        const bool set = (*hp & (1u << (static_cast<uint32_t>(tx) % 8u))) != 0u;
        uint32_t pixel = set ? p.foreground_color : p.background_color;
        if (p.format == 1u) pixel = set ? 0xFFFFFFFFu : 0xFF000000u;
        if (p.swap_rb) pixel = (pixel & 0xFF00FF00u) |
            ((pixel & 0x00FF0000u) >> 16) | ((pixel & 0x000000FFu) << 16);
        return p.srgb ? Imx51Gpu2dSrgbToLinear(pixel) : pixel;
    }
    uint32_t pixel;
    if (packed4) {
        const uint32_t alpha = ((*hp >> ((static_cast<uint32_t>(tx) % 2u) * 4u)) & 15u) * 17u;
        pixel = (alpha << 24) | 0xFFFFFFu;
    } else if (packed8) {
        const uint32_t channel = *hp;
        pixel = p.format == 3u
            ? 0xFF000000u | (channel * 0x010101u) : (channel << 24) | 0xFFFFFFu;
    } else if (packed16) {
        pixel = Gpu2dDecodeTexture16(*reinterpret_cast<const uint16_t*>(hp), p.format);
    } else {
        pixel = *reinterpret_cast<const uint32_t*>(hp);
        if (p.format == 13u) pixel = (pixel >> 8) | (pixel << 24);
    }
    if (p.swap_rb) pixel = (pixel & 0xFF00FF00u) |
        ((pixel & 0x00FF0000u) >> 16) | ((pixel & 0x000000FFu) << 16);
    return p.srgb ? Imx51Gpu2dSrgbToLinear(pixel) : pixel;
}

uint32_t Imx51Gpu2dGradwSampler::Sample(const Gpu2dGradwPaint& p,
                                        int32_t px, int32_t py) const {
    const uint32_t texel = SampleTexel(p, px, py);
    return p.premultiply ? texel : imx51_g2d_blend::PremultArgb(texel);
}

uint32_t Imx51Gpu2dGradwSampler::SampleTexel(const Gpu2dGradwPaint& p,
                                                int32_t px, int32_t py) const {
    float s, t;
    EvalProgram(p, px, py, s, t);
    uint32_t texel;
    if (p.bilinear) {
        /* Half-texel centres (GPU texel-centre convention), clamp at edges. */
        // One-row textures address horizontal texel centers at integers.
        const float center_offset = p.texh == 1 ? 0.0f : 0.5f;
        const float su = s * p.texw - center_offset, tv = t * p.texh - 0.5f;
        const int32_t x0 = static_cast<int32_t>(std::floor(su));
        const int32_t y0 = static_cast<int32_t>(std::floor(tv));
        const float fxr = su - static_cast<float>(x0), fyr = tv - static_cast<float>(y0);
        texel = Lerp(Lerp(Fetch(p, x0, y0), Fetch(p, x0 + 1, y0), fxr),
                     Lerp(Fetch(p, x0, y0 + 1), Fetch(p, x0 + 1, y0 + 1), fxr), fyr);
    } else {
        // Approximate point-sampler subtexel precision before selecting the texel.
        const float sx = std::round(s * p.texw * 256.0f) / 256.0f;
        const float ty = std::round(t * p.texh * 256.0f) / 256.0f;
        texel = Fetch(p, static_cast<int32_t>(std::floor(sx)),
                      static_cast<int32_t>(std::floor(ty)));
    }
    return texel;
}

}

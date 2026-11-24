#include "imx51_gpu2d_pixel_blend.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "imx51_gpu2d_blend.h"
#include "imx51_gpu2d_rasterizer.h"
#include "imx51_id.h"

#include <algorithm>
#include <cstdint>

namespace cerfcore {

REGISTER_SERVICE(Imx51Gpu2dPixelBlend);

using namespace imx51_g2d_blend;

bool Imx51Gpu2dPixelBlend::ShouldRegister() {
    return emu_.Get<BoardContext>().GetSocId() == SocId::Imx51;
}

void Imx51Gpu2dPixelBlend::HaltBlend(const char* why, uint32_t prog) const {
    emu_.Get<Fatal>().Die("[GPU2D-RAST] %s prog=0x%08X - not modeled", why, prog);
}

bool Imx51Gpu2dPixelBlend::BlendProgRefsDest(uint32_t prog) {
    for (uint32_t i = 0; i < 4u; ++i)
        if (!Const(prog, i) && Src(prog, i) == kSrcDest) return true;
    return false;
}

void Imx51Gpu2dPixelBlend::ValidateBlendProg(uint32_t prog) const {
    if (Dst(prog, 0) != 1u || Dst(prog, 1) != 0u || Dst(prog, 2) != 0u)
        HaltBlend("blend multi-TEMP routing", prog);
    for (uint32_t i = 0; i < 4u; ++i) {
        if (!Const(prog, i) && Src(prog, i) > kSrcDest)
            HaltBlend("blend IMAGE/TEMP operand", prog);
    }
}

uint32_t Imx51Gpu2dPixelBlend::BlendPixel(const Gpu2dFillTarget& t, uint32_t src, uint32_t dst,
                                          float cov, uint32_t image) const {
    if (t.premult_dst)
        dst = PremultArgb(dst);
    const uint32_t inputs[] = {0u, src, dst};
    uint32_t a, r, g, b;
    if (t.generic_blend) {
        const uint32_t out = AddProgram(t.alpha_program, t.alpha_passes,
                                        t.color_program, t.color_passes, t.blend_const,
                                        t.image_paint ? t.argb : src, dst,
                                        t.image_paint ? src : image);
        a = out >> 24;
        r = (out >> 16) & 0xFFu;
        g = (out >> 8) & 0xFFu;
        b = out & 0xFFu;
    } else if (t.color_transform) {
        const uint32_t alpha[] = {t.prog_a, t.prog_a1}, color[] = {t.prog_c, t.prog_c1};
        const uint32_t out = AddProgram(alpha, 2u, color, 2u, t.blend_const, src, dst);
        a = out >> 24;
        r = (out >> 16) & 0xFFu;
        g = (out >> 8) & 0xFFu;
        b = out & 0xFFu;
    } else {
        a = BlendChannel(t.prog_a, 24u, inputs, t.blend_const);
        r = BlendChannel(t.prog_c, 16u, inputs, t.blend_const);
        g = BlendChannel(t.prog_c, 8u, inputs, t.blend_const);
        b = BlendChannel(t.prog_c, 0u, inputs, t.blend_const);
    }
    uint32_t argb = (a << 24) | (r << 16) | (g << 8) | b;
    if (cov < 1.0f) {
        const uint32_t coverage_dst = t.oo_alpha && !t.premult_dst ? PremultArgb(dst) : dst;
        uint32_t covered = 0;
        for (uint32_t shift = 0; shift < 32u; shift += 8u) {
            const float result_channel = static_cast<float>((argb >> shift) & 255u);
            const float dest_channel = static_cast<float>((coverage_dst >> shift) & 255u);
            const float mixed = result_channel * cov + dest_channel * (1.0f - cov);
            const uint32_t value = static_cast<uint32_t>(mixed + 0.5f);
            covered |= value << shift;
        }
        argb = covered;
    }
    return t.oo_alpha ? UnpremultArgb(argb) : argb;
}

}

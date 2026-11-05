#include "../src/socs/imx51/imx51_gpu2d_blend.h"
#include "../src/socs/imx51/imx51_gpu2d_pixel_blend.h"
#include "../src/socs/imx51/imx51_gpu2d_rasterizer.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include <cstdio>
#include <cstdlib>

std::string_view cerfcore::BoardContext::GetSocId() const { return {}; }

int main() {
    cerfcore::CerfConfig config;
    cerfcore::CerfEmulator emu(config, 0, nullptr);
    cerfcore::Imx51Gpu2dPixelBlend blend(emu);
    cerfcore::Gpu2dFillTarget target{};
    target.oo_alpha = true;
    target.prog_c = 0x0281A804u;
    for (uint32_t rgb : {0u, 1u, 0x010203u, 0xFFFFFFu}) {
        if (blend.BlendPixel(target, rgb, 0u, 1.0f) != 0u) std::abort();
        target.oo_alpha = false;
        if (blend.BlendPixel(target, rgb, 0u, 1.0f) != rgb) std::abort();
        target.oo_alpha = true;
    }
    for (uint32_t alpha = 0u; alpha < 256u; ++alpha) {
        for (uint32_t color = 0u; color < 256u; ++color) {
            uint32_t expected = 0u;
            if (alpha != 0u) {
                expected = static_cast<uint32_t>(color * 255.0 / alpha + 0.5);
                if (expected > 255u) expected = 255u;
            }
            if (cerfcore::imx51_g2d_blend::UnpremultChannel(color, alpha) != expected)
                std::abort();
        }
    }
    std::puts("imx51_gpu2d_blend_test passed: all channel/alpha pairs");
}

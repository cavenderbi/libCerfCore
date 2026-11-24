#include "../src/socs/imx51/imx51_gpu2d_blend.h"

#include <cstdint>
#include <cstdio>

int main() {
    using cerfcore::imx51_g2d_blend::UnpremultArgb;
    for (uint32_t color = 0; color <= 0xffffffu; ++color) {
        if (UnpremultArgb(color) != 0u || UnpremultArgb(0xff000000u | color) != (0xff000000u | color)) {
            std::printf("zero or opaque alpha failed: %08x\n", color);
            return 1;
        }
    }
    for (uint32_t alpha = 1; alpha < 255; ++alpha) {
        for (uint32_t channel = 0; channel <= 255; ++channel) {
            uint32_t expected = (channel * 255u + alpha / 2u) / alpha;
            if (expected > 255u) expected = 255u;
            const uint32_t pixel = (alpha << 24) | (channel << 16) | (channel << 8) | channel;
            const uint32_t output = (alpha << 24) | (expected << 16) | (expected << 8) | expected;
            if (UnpremultArgb(pixel) != output) {
                std::printf("partial alpha failed: %08x\n", pixel);
                return 1;
            }
        }
    }
    return 0;
}

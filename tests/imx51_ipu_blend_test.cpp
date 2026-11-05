#include "../src/socs/imx51/imx51_ipu_blend.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace {

uint32_t Oracle(uint32_t fg, uint16_t bg) {
    const uint32_t r = ((bg >> 11) << 3) | ((bg >> 11) >> 2);
    const uint32_t g = (((bg >> 5) & 63u) << 2) | (((bg >> 5) & 63u) >> 4);
    const uint32_t b = ((bg & 31u) << 3) | ((bg & 31u) >> 2);
    const uint32_t a = fg >> 24;
    const uint32_t i = 255u - a;
    return 0xFF000000u |
           (((a * ((fg >> 16) & 255u) + i * r + 127u) / 255u) << 16) |
           (((a * ((fg >> 8) & 255u) + i * g + 127u) / 255u) << 8) |
           ((a * (fg & 255u) + i * b + 127u) / 255u);
}

}

int main() {
    constexpr uint32_t count = 65536u;
    std::vector<uint8_t> fg(count * 4u + 16u), bg(count * 2u + 16u);
    std::vector<uint32_t> out(count + 2u), expected(count + 2u);
    std::mt19937 random(4321u);
    for (uint32_t a = 0u; a < 256u; ++a) {
        for (uint32_t p = 0u; p < count; ++p) {
            const uint32_t f = (random() & 0xFFFFFFu) | (a << 24);
            const uint16_t b = static_cast<uint16_t>(p);
            cerfengine::le::Put32(fg.data() + 1u + p * 4u, f);
            cerfengine::le::Put16(bg.data() + 1u + p * 2u, b);
            expected[p] = Oracle(f, b);
        }
        cerfcore::imx51_ipu::BlendRow(out.data(), fg.data() + 1u, bg.data() + 1u, count);
        if (!std::equal(expected.begin(), expected.begin() + count, out.begin())) std::abort();
    }
    for (uint32_t width = 0u; width < 40u; ++width) {
        for (uint32_t offset = 0u; offset < 8u; ++offset) {
            for (bool black : {false, true}) {
                std::fill(out.begin(), out.end(), 0x12345678u);
                expected = out;
                for (uint32_t x = 0u; x < width; ++x) {
                    const uint32_t f = cerfengine::le::U32(fg.data() + offset + x * 4u);
                    const uint16_t b = black ? 0u : cerfengine::le::U16(bg.data() + offset + x * 2u);
                    expected[x + 1u] = Oracle(f, b);
                }
                cerfcore::imx51_ipu::BlendRow(out.data() + 1u, fg.data() + offset,
                                             black ? nullptr : bg.data() + offset, width);
                if (out != expected) std::abort();
            }
        }
    }
    std::puts("imx51_ipu_blend_test passed: 16,777,216 pixel cases plus row boundaries");
}

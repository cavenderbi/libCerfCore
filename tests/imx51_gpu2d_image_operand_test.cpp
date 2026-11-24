#include "../src/socs/imx51/imx51_gpu2d_blend.h"
#include <cstdio>
int main() {
    using namespace cerfcore::imx51_g2d_blend;
    const uint32_t constants[8] = {};
    const uint32_t alpha[] = {0x00190320u, 0x00852004u};
    const uint32_t color[] = {0x00190020u, 0x00852004u};
    const uint32_t paint = 0x80604020u, image = 0x40201008u;
    const uint32_t result = AddProgram(alpha, 2, color, 2, constants, paint, 0u, image);
    if (result != 0x200c0401u) {
        std::printf("image operand: expected 200c0401, got %08x\n", result);
        return 1;
    }
    const uint32_t dest = 0x80402010u;
    const uint32_t over_a[] = {0x065900e0u, 0x0a85a004u};
    const uint32_t over_c[] = {0x065904e0u, 0x0c85a004u};
    uint32_t expected = 0;
    for (uint32_t shift = 0; shift < 32; shift += 8) {
        const uint32_t p = (paint >> shift) & 255u;
        const uint32_t i = (image >> shift) & 255u;
        const uint32_t d = (dest >> shift) & 255u;
        const uint32_t value = Mul(p, i) + Mul(d, 255u - Mul(paint >> 24, i));
        expected |= (value > 255u ? 255u : value) << shift;
    }
    const uint32_t actual = AddProgram(over_a, 2, over_c, 2, constants, paint, dest, image);
    if (actual != expected) {
        std::printf("image src-over: expected %08x, got %08x\n", expected, actual);
        return 1;
    }
    return 0;
}

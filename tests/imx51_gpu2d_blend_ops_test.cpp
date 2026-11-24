#include "../src/socs/imx51/imx51_gpu2d_blend.h"
#include "../src/socs/imx51/imx51_gpu2d_pixel_blend.h"
#include "../src/socs/imx51/imx51_gpu2d_rasterizer.h"
#include <cerfcore/core/cerf_emulator.h>

#include <cstdint>
#include <cstdio>

int main() {
    using cerfcore::imx51_g2d_blend::AddProgram;
    using cerfcore::imx51_g2d_blend::BlendChannel;
    // TEMP0 = SOURCE * 1 (OP) DEST * 1.
    const uint32_t program = 0x00812004u | (2u << 22) | (1u << 15);
    const uint32_t constants[8] = {};
    cerfcore::CerfConfig config{};
    cerfcore::CerfEmulator emulator(config, 0, nullptr);
    emulator.CreateAllServices();
    cerfcore::Imx51Gpu2dPixelBlend blender(emulator);
    cerfcore::Gpu2dFillTarget target{};
    for (uint32_t source = 0; source < 256; ++source) {
        for (uint32_t dest = 0; dest < 256; ++dest) {
            const uint32_t expected[] = {
                source + dest > 255u ? 255u : source + dest,
                source > dest ? source - dest : 0u,
                source < dest ? source : dest,
                source > dest ? source : dest,
            };
            for (uint32_t operation = 0; operation < 4; ++operation) {
                const uint32_t instruction = program | operation;
                const uint32_t source_pixel = source * 0x01010101u;
                const uint32_t dest_pixel = dest * 0x01010101u;
                const uint32_t result = AddProgram(&instruction, 1, &instruction, 1,
                                                   constants, source_pixel, dest_pixel);
                target.prog_a = target.prog_c = instruction;
                blender.ValidateBlendProg(instruction);
                const uint32_t actual_pixel = blender.BlendPixel(target, source_pixel, dest_pixel, 1.0f);
                const uint32_t inputs[] = {0u, source_pixel, dest_pixel};
                const uint32_t channel = BlendChannel(instruction, 0, inputs, constants);
                if (channel != expected[operation] || result != expected[operation] * 0x01010101u ||
                    actual_pixel != result) {
                    std::printf("blend operation %u source %u dest %u produced %08x\n",
                                operation, source, dest, result);
                    return 1;
                }
            }
        }
    }
    target.prog_a = target.prog_c = 0x00812004u;
    const uint32_t source_pixel = 0x80604020u;
    const uint32_t dest_pixel = 0x80402010u;
    const uint32_t half_covered = 0x80503018u;
    if (blender.BlendPixel(target, source_pixel, dest_pixel, 0.5f) != half_covered ||
        blender.BlendPixel(target, source_pixel, dest_pixel, 0.0f) != dest_pixel ||
        blender.BlendPixel(target, source_pixel, dest_pixel, 1.0f) != source_pixel) {
        std::printf("source blend coverage did not preserve destination\n");
        return 1;
    }
    target.oo_alpha = true;
    const uint32_t straight_dest = 0x80804020u;
    const uint32_t expected_straight = cerfcore::imx51_g2d_blend::UnpremultArgb(half_covered);
    if (blender.BlendPixel(target, source_pixel, straight_dest, 0.5f) != expected_straight) {
        std::printf("straight-alpha coverage did not preserve destination\n");
        return 1;
    }
    target.premult_dst = true;
    if (blender.BlendPixel(target, source_pixel, straight_dest, 0.5f) != expected_straight) {
        std::printf("premultiplied destination coverage did not preserve destination\n");
        return 1;
    }
    target = {};
    target.generic_blend = true;
    target.alpha_passes = target.color_passes = 2;
    target.alpha_program[0] = target.color_program[0] = 0x00812008u;
    target.alpha_program[1] = target.color_program[1] = 0x00852004u;
    if (blender.BlendPixel(target, source_pixel, dest_pixel, 1.0f) != source_pixel) {
        std::printf("multi-pass vector blending did not route TEMP1\n");
        return 1;
    }
    target.image_paint = true;
    target.argb = 0xFFFFFFFFu;
    target.alpha_program[0] = 0x00190320u; target.color_program[0] = 0x00190020u;
    target.alpha_program[1] = target.color_program[1] = 0x00852004u;
    if (blender.BlendPixel(target, source_pixel, dest_pixel, 1.0f) != source_pixel) {
        std::printf("vector image paint did not use separate texture and paint\n");
        return 1;
    }
    return 0;
}

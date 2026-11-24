#include "../src/socs/imx51/imx51_gpu2d_command_engine.h"
#include "../src/socs/imx51/imx51_gpu2d_direct2d.h"
#include "../src/socs/imx51/imx51_gpu2d_gradw_sampler.h"
#include "../src/socs/imx51/imx51_gpu2d_image_paint.h"
#include "../src/socs/imx51/imx51_gpu2d_pixel_blend.h"
#include "../src/socs/imx51/imx51_gpu2d_rasterizer.h"
#include "../src/socs/imx51/imx51_gpu2d_stroker.h"
#include "../src/socs/imx51/imx51_gpu2d_vg_fill.h"
#include "../src/boards/page_table_builder.h"
#include "../src/peripherals/peripheral_dispatcher.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/execution/jit_runner.h>
#include <cerfcore/state/state_stream.h>
#include <cstdio>
#include <cstring>
#include <memory>

class TestGradwSampler final : public cerfcore::Imx51Gpu2dGradwSampler {
public:
    using Imx51Gpu2dGradwSampler::Imx51Gpu2dGradwSampler;
    bool ShouldRegister() override { return true; }
};

class TestRasterizer final : public cerfcore::Imx51Gpu2dRasterizer {
public:
    using Imx51Gpu2dRasterizer::Imx51Gpu2dRasterizer;
    bool ShouldRegister() override { return true; }
};

class TestPixelBlend final : public cerfcore::Imx51Gpu2dPixelBlend {
public:
    using Imx51Gpu2dPixelBlend::Imx51Gpu2dPixelBlend;
    bool ShouldRegister() override { return true; }
};

class CopyTestMemory final : public cerfcore::PageTableBuilder {
public:
    using PageTableBuilder::PageTableBuilder;
    uint32_t VaToPa(uint32_t address) const override { return address; }
    std::vector<cerfcore::DramRegion> CachedDramRegions() const override { return {}; }
    std::vector<cerfcore::DramRegion> MappedVaSpans() const override { return {}; }
    std::vector<cerfcore::BackedRegion> BackedMemoryRegions() const override {
        return {{0x1000u, 0x1000u, 0x3000u, cerfcore::MemoryKind::kRam}};
    }
};
int main() {
    using cerfcore::Imx51Gpu2dFillColor;
    struct Case { unsigned color, format, expected; };
    const Case cases[] = {
        {0xFF0000FFu, 7u, 0xFF0000FFu},
        {0xFF0000FFu, 13u, 0x0000FFFFu},
        {0xFFFF0000u, 7u, 0xFFFF0000u},
        {0xFFFF0000u, 13u, 0xFF0000FFu},
        {0x12345678u, 7u, 0x12345678u},
        {0x12345678u, 13u, 0x34567812u},
        {0x12345678u, 6u, 0x12345678u},
    };
    for (const auto& entry : cases) {
        const auto value = Imx51Gpu2dFillColor(entry.color, entry.format);
        if (value != entry.expected) {
            std::printf("format %u color %08x: got %08x expected %08x\n",
                        entry.format, entry.color, value, entry.expected);
            return 1;
        }
    }
    if (cerfcore::Imx51Gpu2dCopyColor(0x34567812u, 13u) != 0x12345678u ||
        cerfcore::Imx51Gpu2dCopyColor(0x12345678u, 7u) != 0x12345678u) {
        std::printf("copy source channel order is wrong\n");
        return 1;
    }
    if (cerfcore::Imx51Gpu2dSrgbToLinear(0x7F4080C0u) != 0x7F0D3786u ||
        cerfcore::Imx51Gpu2dSrgbToLinear(0x00000000u) != 0x00000000u ||
        cerfcore::Imx51Gpu2dSrgbToLinear(0xFFFFFFFFu) != 0xFFFFFFFFu) {
        std::printf("sRGB-to-linear conversion is wrong\n");
        return 1;
    }
    for (const uint32_t format : {4u, 5u, 6u, 11u, 12u}) {
        for (uint32_t pixel = 0; pixel <= 0xFFFFu; ++pixel) {
            const uint32_t expanded = cerfcore::Gpu2dDecodeTexture16(static_cast<uint16_t>(pixel), format);
            if (cerfcore::Gpu2dEncodeTexture16(expanded, format) != pixel) {
                std::printf("packed format %u failed round-trip for %04x\n", format, pixel);
                return 1;
            }
        }
    }
    cerfcore::CerfConfig config{};
    cerfcore::CerfEmulator emulator(config, 0, nullptr);
    emulator.AddCandidate<cerfcore::Fatal>(std::make_unique<cerfcore::Fatal>(emulator), false);
    emulator.AddCandidate<cerfcore::JitRunner>(std::make_unique<cerfcore::JitRunner>(emulator), false);
    emulator.AddCandidate<cerfcore::PeripheralDispatcher>(
        std::make_unique<cerfcore::PeripheralDispatcher>(emulator), false);
    emulator.AddCandidate<cerfcore::PageTableBuilder>(std::make_unique<CopyTestMemory>(emulator), false);
    emulator.AddCandidate<cerfcore::EmulatedMemory>(
        std::make_unique<cerfcore::EmulatedMemory>(emulator), false);
    emulator.AddCandidate<cerfcore::Imx51Gpu2dGradwSampler>(
        std::make_unique<TestGradwSampler>(emulator), false);
    emulator.AddCandidate<cerfcore::Imx51Gpu2dPixelBlend>(
        std::make_unique<TestPixelBlend>(emulator), false);
    emulator.AddCandidate<cerfcore::Imx51Gpu2dRasterizer>(
        std::make_unique<TestRasterizer>(emulator), false);
    auto& memory = emulator.Get<cerfcore::EmulatedMemory>();
    memory.WriteWord(0x2000u, 0x4080C07Fu);

    // Compare smooth VGV2 commands with explicit reflected controls across curve types.
    cerfcore::Imx51Gpu2dCommandEngine command_engine(emulator);
    auto& command_rasterizer = emulator.Get<cerfcore::Imx51Gpu2dRasterizer>();
    auto make_curve_path = [&](bool cubic_first, bool smooth_second) {
        command_engine.PowerOnReset();
        command_rasterizer.ClearPath();
        command_engine.WriteReg(0x50u, 0x3F800000u);
        command_engine.WriteReg(0x53u, 0x3F800000u);
        std::vector<uint32_t> stream;
        auto geometry = [&](uint32_t action, uint32_t address, std::initializer_list<float> coords) {
            const uint32_t count = static_cast<uint32_t>(coords.size());
            stream.push_back(0x7B000000u | (action << 20) | (count << 16) | (count << 8) | address);
            for (float coordinate : coords) {
                uint32_t bits;
                std::memcpy(&bits, &coordinate, sizeof(bits));
                stream.push_back(bits);
            }
        };
        geometry(1u, 0x46u, {1.0f, 1.0f});
        if (cubic_first) {
            geometry(4u, 0x42u, {2.0f, 1.0f, 3.0f, 2.0f, 4.0f, 3.0f});
            if (smooth_second) geometry(7u, 0x46u, {7.0f, 5.0f});
            else geometry(5u, 0x44u, {5.0f, 4.0f, 7.0f, 5.0f});
        } else {
            geometry(5u, 0x44u, {2.0f, 1.0f, 4.0f, 3.0f});
            if (smooth_second) geometry(6u, 0x44u, {6.0f, 5.0f, 7.0f, 5.0f});
            else geometry(4u, 0x42u, {6.0f, 5.0f, 6.0f, 5.0f, 7.0f, 5.0f});
        }
        for (size_t i = 0; i < stream.size(); ++i) memory.WriteWord(0x2000u + static_cast<uint32_t>(i) * 4u, stream[i]);
        command_engine.WriteReg(0x75u, 0x2000u);
        command_engine.WriteReg(0x76u, static_cast<uint32_t>(stream.size()) | (1u << 12) | (1u << 15));
        command_engine.WriteReg(0x70u, 1u);
        return command_rasterizer.PathPoints();
    };
    for (bool cubic_first : {false, true}) {
        const auto explicit_path = make_curve_path(cubic_first, false);
        const auto smooth_path = make_curve_path(cubic_first, true);
        if (smooth_path != explicit_path) {
            std::printf("mixed smooth curve does not reflect the previous control point\n"); return 1;
        }
    }
    command_rasterizer.ClearPath();
    memory.WriteWord(0x2000u, 0x4080C07Fu);

    uint32_t regs[0x100] = {};
    regs[0] = 0x1000u; regs[1] = 0x7000u;
    regs[2] = 0x2000u; regs[3] = 0x2D000u;
    regs[0xD] = 0x404u; regs[0xE] = 2u; regs[0xF] = 2u;
    regs[0x10] = 0xFFFFFFu; regs[0xF1] = 0x10001u;
    cerfcore::Imx51Gpu2dDirect2d direct(emulator);
    auto& gradientRasterizer = emulator.Get<cerfcore::Imx51Gpu2dRasterizer>();
    gradientRasterizer.OnReady();
    cerfcore::Imx51Gpu2dVgFill vgFill(emulator);
    uint32_t gradientRegs[0x100] = {};
    gradientRegs[0] = 0x1000u; gradientRegs[1] = 0x7000u;
    gradientRegs[0x10] = 0xFFFFFFu; gradientRegs[0xF] = 0x11u;
    gradientRegs[0x11] = 0x60u; gradientRegs[0x14] = gradientRegs[0x18] = 0x00812004u;
    gradientRegs[0x5E] = 0x3F800000u;
    gradientRegs[0xD0] = 0x40u; gradientRegs[0xD1] = 0x7000u;
    gradientRegs[0xD2] = 0x801u; gradientRegs[0xD3] = 0x2000u;
    memory.WriteWord(0x2000u, 0x80402010u);
    auto drawGradient = [&] {
        gradientRasterizer.MoveTo(0.0f, 0.0f, true);
        gradientRasterizer.LineTo(1.0f, 0.0f);
        gradientRasterizer.LineTo(1.0f, 1.0f);
        gradientRasterizer.LineTo(0.0f, 1.0f);
        vgFill.Flush(gradientRegs, false);
    };
    drawGradient();
    if (memory.ReadWord(0x1000u) != 0x80804020u) {
        std::printf("premultiplied gradient texture failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    gradientRegs[0xD1] |= 1u << 23;
    drawGradient();
    if (memory.ReadWord(0x1000u) != 0x80402010u) {
        std::printf("straight gradient texture failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    memory.WriteWord(0x2000u, 0x4080C07Fu);
    uint32_t fill_regs[0x100] = {};
    fill_regs[0] = 0x1000u; fill_regs[1] = 0x7000u;
    fill_regs[0xD] = 0x404u; fill_regs[0x10] = 0xFFFFFFu;
    fill_regs[0xF1] = 0x10001u; fill_regs[0xFF] = 0xFFE7E9C2u;
    direct.Fill(fill_regs);
    if (memory.ReadWord(0x1000u) != 0xFFE7E9C2u) {
        std::printf("ARGB fill color failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    fill_regs[0] = 0x1000u; fill_regs[1] = 0x20D000u;
    fill_regs[0xD] = 0x404u; fill_regs[0x10] = 0xFFFFFFu;
    fill_regs[0xF1] = 0x10001u; fill_regs[0xFF] = 0xFF563412u;
    direct.Fill(fill_regs);
    if (memory.ReadWord(0x1000u) != 0x123456FFu) {
        std::printf("destination red/blue swap fill failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    fill_regs[1] = 0x4000u; fill_regs[0xFF] = 0xA1B2C3D4u;
    memory.WriteWord(0x1000u, 0x12345678u); direct.Fill(fill_regs);
    if (memory.ReadWord(0x1000u) != 0x1234ABCDu) {
        std::printf("ARGB4444 fill or adjacent pixel failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    fill_regs[1] = 0x5000u; fill_regs[0xFF] = 0xFFFF0000u;
    memory.WriteWord(0x1000u, 0x12345678u); direct.Fill(fill_regs);
    if (memory.ReadWord(0x1000u) != 0x1234FC00u) {
        std::printf("ARGB1555 fill or adjacent pixel failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    regs[1] = 0x7000u; regs[3] = 0x4000u; regs[0xE] = 2u;
    memory.WriteWord(0x2000u, 0x0000ABCDu); direct.Copy(regs, 0);
    if (memory.ReadWord(0x1000u) != 0xAABBCCDDu) {
        std::printf("ARGB4444 source copy failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    regs[3] = 0x5000u; memory.WriteWord(0x2000u, 0x0000FC00u); direct.Copy(regs, 0);
    if (memory.ReadWord(0x1000u) != 0xFFFF0000u) {
        std::printf("ARGB1555 source copy failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    regs[3] = 0x2D000u; memory.WriteWord(0x2000u, 0x4080C07Fu);
    direct.Copy(regs, 0);
    if (memory.ReadWord(0x1000u) != 0x7F0D3786u) {
        std::printf("RGBA linear copy produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    regs[0x11] = 0x20u; regs[0x14] = 0x12010u; regs[0x18] = 0x90110u;
    direct.Copy(regs, 0);
    if (memory.ReadWord(0x1000u) != 0x7F061B43u) {
        std::printf("premultiplied RGBA linear copy produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    regs[3] = 0xD000u; regs[0xE] = 0x802u; regs[0x11] = 0u;
    memory.WriteWord(0x2000u, 0x0D37867Fu); direct.Copy(regs, 0u);
    if (memory.ReadWord(0x1000u) != 0x7F4080C0u) {
        std::printf("linear-to-sRGB copy produced %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    regs[1] = 0xD000u; regs[0xE] = 2u;
    memory.WriteWord(0x2000u, 0x11223344u); direct.Copy(regs, 0u);
    if (memory.ReadWord(0x1000u) != 0x11223344u) {
        std::printf("RGBA destination copy produced %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    regs[1] = 0x7000u; regs[0x11] = 0x20u;
    regs[0x14] = 0x3010u; regs[0x18] = 0x12010u;
    memory.WriteWord(0x2000u, 0x11223344u); direct.Copy(regs, 0u);
    if (memory.ReadWord(0x1000u) != 0xFF112233u) {
        std::printf("opaque source copy produced %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    regs[1] = 0x6000u; regs[0x11] = 0u;
    memory.WriteWord(0x2000u, 0xFF0000FFu); memory.WriteWord(0x1000u, 0u); direct.Copy(regs, 0u);
    if (memory.ReadHalf(0x1000u) != 0xF800u) {
        std::printf("RGB565 destination copy failed\n"); return 1;
    }
    regs[1] = 0xC000u; memory.WriteWord(0x2000u, 0xFF000044u); direct.Copy(regs, 0u);
    if (memory.ReadHalf(0x1000u) != 0xF800u) {
        std::printf("RGBA5551 destination copy failed\n"); return 1;
    }
    regs[1] = 0xB000u; direct.Copy(regs, 0u);
    if (memory.ReadHalf(0x1000u) != 0xF004u) {
        std::printf("RGBA4444 destination copy failed\n"); return 1;
    }
    regs[1] = 0x4000u; direct.Copy(regs, 0u);
    if (memory.ReadHalf(0x1000u) != 0x4F00u) {
        std::printf("ARGB4444 destination copy failed\n"); return 1;
    }
    regs[1] = 0x5000u; direct.Copy(regs, 0u);
    if (memory.ReadHalf(0x1000u) != 0x7C00u) {
        std::printf("ARGB1555 destination copy failed\n"); return 1;
    }
    regs[1] = 0x20D000u; memory.WriteWord(0x2000u, 0xFF000044u); direct.Copy(regs, 0u);
    if (memory.ReadWord(0x1000u) != 0x0000FF44u) {
        std::printf("destination red-blue swap copy failed\n"); return 1;
    }
    uint32_t dither_copy[0x100] = {};
    dither_copy[0] = 0x1000u; dither_copy[1] = 0x6001u;
    dither_copy[2] = 0x2000u; dither_copy[3] = 0x7003u;
    dither_copy[8] = 3u << 11; dither_copy[9] = 3u << 11;
    dither_copy[0xD] = 0x404u; dither_copy[0xE] = 0x402u;
    dither_copy[0xF] = 2u; dither_copy[0x10] = 0xFFFFFFu;
    dither_copy[0xF1] = (4u << 16) | 4u;
    for (uint32_t pixel = 0; pixel < 16u; ++pixel)
        memory.WriteWord(0x2000u + pixel * 4u, 0xFF828282u);
    direct.Copy(dither_copy, 0u);
    const uint16_t expected_dither[16] = {
        0x8430u, 0x8410u, 0x8410u, 0x8410u,
        0x8410u, 0x8410u, 0x7C0Fu, 0x8410u,
        0x8410u, 0x8410u, 0x8430u, 0x8410u,
        0x7C0Fu, 0x8410u, 0x7C0Fu, 0x8410u,
    };
    for (uint32_t pixel = 0; pixel < 16u; ++pixel) {
        if (memory.ReadHalf(0x1000u + pixel * 2u) != expected_dither[pixel]) {
            std::printf("RGB565 dither mismatch at pixel %u\n", pixel); return 1;
        }
    }
    struct DitherFormatCase { uint32_t config; uint16_t high, low; };
    const DitherFormatCase dither_formats[] = {
        {0xB001u, 0x888Fu, 0x777Fu}, {0xC001u, 0x8421u, 0x7BDFu},
        {0x4001u, 0xF888u, 0xF777u}, {0x5001u, 0xC210u, 0xBDEFu},
    };
    for (const auto& entry : dither_formats) {
        dither_copy[1] = entry.config;
        direct.Copy(dither_copy, 0u);
        if (memory.ReadHalf(0x1000u) != entry.high || memory.ReadHalf(0x1018u) != entry.low) {
            std::printf("packed dither mismatch for config %08x\n", entry.config); return 1;
        }
    }
    uint32_t premult_dither[0x100] = {};
    premult_dither[0] = 0x1000u; premult_dither[1] = 0x5001u;
    premult_dither[2] = 0x2000u; premult_dither[3] = 0x7001u;
    premult_dither[8] = 1u << 11; premult_dither[9] = 0u;
    premult_dither[0xD] = 0x404u; premult_dither[0xE] = 0x402u;
    premult_dither[0xF] = 2u; premult_dither[0x10] = 0xFFFFFFu;
    premult_dither[0x11] = 0x20u; premult_dither[0x14] = 0x12010u;
    premult_dither[0x18] = 0x90110u; premult_dither[0xF1] = 0x20001u;
    memory.WriteWord(0x2000u, 0x40FFFFFFu); memory.WriteWord(0x2004u, 0x40FFFFFFu);
    direct.Copy(premult_dither, 0u);
    if (memory.ReadHalf(0x1000u) != 0xA108u || memory.ReadHalf(0x1002u) != 0u) {
        std::printf("premultiplied alpha dither mismatch: %04x %04x\n", memory.ReadHalf(0x1000u), memory.ReadHalf(0x1002u)); return 1;
    }
    constexpr uint8_t thresholds[16] = {0u,8u,2u,10u,12u,4u,14u,6u,3u,11u,1u,9u,15u,7u,13u,5u};
    premult_dither[3] = 0x7003u;
    premult_dither[8] = premult_dither[9] = 3u << 11;
    premult_dither[0xF1] = (4u << 16) | 4u;
    for (uint32_t format : {5u, 12u}) {
        premult_dither[1] = (format << 12) | 1u;
        for (uint32_t alpha = 0; alpha < 256u; ++alpha) {
            for (uint32_t pixel = 0; pixel < 16u; ++pixel) memory.WriteWord(0x2000u + pixel * 4u, (alpha << 24) | 0x00FFFFFFu);
            direct.Copy(premult_dither, 0u);
            for (uint32_t pixel = 0; pixel < 16u; ++pixel) {
                const uint16_t packed = memory.ReadHalf(0x1000u + pixel * 2u);
                const bool expected_alpha = alpha == 255u || alpha * 16u > thresholds[pixel] * 255u;
                const uint16_t alpha_bit = format == 5u ? 0x8000u : 0x0001u;
                if (((packed & alpha_bit) != 0u) != expected_alpha || (!expected_alpha && packed != 0u)) {
                    std::printf("alpha dither sweep failed: format=%u alpha=%u pixel=%u value=%04x\n", format, alpha, pixel, packed); return 1;
                }
            }
        }
    }
    premult_dither[3] = 0x7001u;
    premult_dither[8] = 1u << 11; premult_dither[9] = 0u;
    premult_dither[0xE] = 2u; premult_dither[0xF1] = 0x20001u;
    memory.WriteWord(0x2000u, 0x7FFFFFFFu); memory.WriteWord(0x2004u, 0x80FFFFFFu);
    for (uint32_t format : {5u, 12u}) {
        premult_dither[1] = (format << 12) | 1u;
        premult_dither[0x11] = 0x20u;
        direct.Copy(premult_dither, 0u);
        const uint16_t opaque = format == 5u ? 0xC210u : 0x8421u;
        if (memory.ReadHalf(0x1000u) != 0u || memory.ReadHalf(0x1002u) != opaque) {
            std::printf("premultiplied 1555 copy threshold failed for format %u\n", format); return 1;
        }
        premult_dither[0x11] = 0u;
        direct.Copy(premult_dither, 0u);
        const uint16_t transparent = format == 5u ? 0x7FFFu : 0xFFFEu;
        if (memory.ReadHalf(0x1000u) != transparent || memory.ReadHalf(0x1002u) != 0xFFFFu) {
            std::printf("straight-alpha 1555 copy lost color for format %u\n", format); return 1;
        }
    }
    for (uint32_t pixel = 0; pixel < 16u; ++pixel) memory.WriteWord(0x2000u + pixel * 4u, 0xFF828282u);
    dither_copy[1] = 0x6001u;
    dither_copy[0xE] = 2u;
    direct.Copy(dither_copy, 0u);
    for (uint32_t pixel = 0; pixel < 16u; ++pixel) {
        if (memory.ReadHalf(0x1000u + pixel * 2u) != 0x8410u) {
            std::printf("RGB565 nondithered copy mismatch at pixel %u\n", pixel); return 1;
        }
    }
    dither_copy[0xE] = 0x402u;
    for (uint32_t pixel = 0; pixel < 16u; ++pixel)
        memory.WriteHalf(0x2000u + pixel * 2u, static_cast<uint16_t>((pixel * 0x1001u) ^ 0x8410u));
    for (uint32_t format : {0x4001u, 0x5001u, 0x6001u, 0xB001u, 0xC001u}) {
        dither_copy[1] = dither_copy[3] = format;
        direct.Copy(dither_copy, 0u);
        for (uint32_t pixel = 0; pixel < 16u; ++pixel) {
            if (memory.ReadHalf(0x1000u + pixel * 2u) != memory.ReadHalf(0x2000u + pixel * 2u)) {
                std::printf("same-format dither changed pixel %u in format %08x\n", pixel, format); return 1;
            }
        }
    }
    for (uint32_t pixel = 0; pixel < 16u; ++pixel) memory.WriteHalf(0x2000u + pixel * 2u, 0x8410u);
    dither_copy[3] = 0x6001u; dither_copy[1] = 0xB001u;
    direct.Copy(dither_copy, 0u);
    if (memory.ReadHalf(0x1000u) != 0x888Fu || memory.ReadHalf(0x1018u) != 0x777Fu) {
        std::printf("RGB565 to RGBA4444 dither mismatch\n"); return 1;
    }
    uint32_t reverse_copy[0x100] = {};
    reverse_copy[0] = reverse_copy[2] = 0x2000u;
    reverse_copy[1] = reverse_copy[3] = 0x7007u;
    reverse_copy[8] = 7u << 11; reverse_copy[9] = 7u << 11;
    reverse_copy[0xD] = 0x404u; reverse_copy[0xE] = 2u;
    reverse_copy[0xF] = 2u; reverse_copy[0x10] = 0xFFFFFFu;
    for (uint32_t i = 0; i < 8u; ++i) memory.WriteWord(0x2000u + i * 4u, i + 1u);
    reverse_copy[0xF0] = 7u << 16; reverse_copy[0xF1] = (0xFFAu << 16) | 1u;
    direct.Copy(reverse_copy, 5u << 16);
    for (uint32_t i = 2; i < 8u; ++i) {
        if (memory.ReadWord(0x2000u + i * 4u) != i - 1u) {
            std::printf("reverse horizontal copy failed at %u\n", i); return 1;
        }
    }
    for (uint32_t i = 0; i < 8u; ++i) memory.WriteWord(0x2000u + i * 32u, i + 1u);
    reverse_copy[0xF0] = 7u; reverse_copy[0xF1] = 0xFFAu;
    direct.Copy(reverse_copy, 5u);
    for (uint32_t i = 2; i < 8u; ++i) {
        if (memory.ReadWord(0x2000u + i * 32u) != i - 1u) {
            std::printf("reverse vertical copy failed at %u\n", i); return 1;
        }
    }
    // Dithering preserves exact values in a same-format packed copy, including overlap.
    for (uint32_t format : {0x4000u, 0x5000u, 0x6000u, 0xB000u, 0xC000u}) {
        reverse_copy[1] = reverse_copy[3] = format | 3u;
        reverse_copy[0xE] = 0x402u;
        for (uint32_t i = 0; i < 8u; ++i) memory.WriteHalf(0x2000u + i * 2u, i + 1u);
        reverse_copy[0xF0] = 7u << 16;
        reverse_copy[0xF1] = (0xFFAu << 16) | 1u;
        direct.Copy(reverse_copy, 5u << 16);
        for (uint32_t i = 2; i < 8u; ++i) {
            if (memory.ReadHalf(0x2000u + i * 2u) != i - 1u) {
                std::printf("reverse packed horizontal dither failed in %08x at %u\n", format, i); return 1;
            }
        }
        reverse_copy[1] = reverse_copy[3] = format | 1u;
        for (uint32_t i = 0; i < 8u; ++i) memory.WriteHalf(0x2000u + i * 8u, i + 1u);
        reverse_copy[0xF0] = 7u;
        reverse_copy[0xF1] = 0xFFAu;
        direct.Copy(reverse_copy, 5u);
        for (uint32_t i = 2; i < 8u; ++i) {
            if (memory.ReadHalf(0x2000u + i * 8u) != i - 1u) {
                std::printf("reverse packed vertical dither failed in %08x at %u\n", format, i); return 1;
            }
        }
    }
    uint32_t reverse_unpremult[0x100] = {};
    reverse_unpremult[0] = 0x1000u; reverse_unpremult[1] = 0x7000u;
    reverse_unpremult[2] = 0x2000u; reverse_unpremult[3] = 0x7000u;
    reverse_unpremult[8] = 0x800u; reverse_unpremult[9] = 0u;
    reverse_unpremult[0xD] = 0x404u; reverse_unpremult[0xE] = 2u;
    reverse_unpremult[0xF] = 2u; reverse_unpremult[0x10] = 0xFFFFFFu;
    reverse_unpremult[0x11] = 0x60u; reverse_unpremult[0x14] = reverse_unpremult[0x18] = 0x12010u;
    reverse_unpremult[0xF0] = 1u << 16; reverse_unpremult[0xF1] = (0xFFEu << 16) | 1u;
    memory.WriteWord(0x2000u, 0x80402010u); memory.WriteWord(0x2004u, 0x80402010u);
    direct.Copy(reverse_unpremult, 1u << 16);
    if (memory.ReadWord(0x1000u) != 0x80804020u || memory.ReadWord(0x1004u) != 0x80804020u) {
        std::printf("reverse copy unpremultiplication failed\n"); return 1;
    }
    uint32_t reverse_mask[0x100] = {};
    reverse_mask[0] = reverse_mask[2] = 0x2000u;
    reverse_mask[1] = reverse_mask[3] = 0xE001u;
    reverse_mask[8] = 7u << 11; reverse_mask[9] = 0u;
    reverse_mask[0xD] = 0x404u; reverse_mask[0xE] = 2u;
    reverse_mask[0xF] = 2u; reverse_mask[0x10] = 0xFFFFFFu;
    reverse_mask[0xF0] = 7u << 16; reverse_mask[0xF1] = (0xFFAu << 16) | 1u;
    for (uint32_t i = 0; i < 8u; ++i) memory.WriteByte(0x2000u + i, i + 1u);
    direct.Copy(reverse_mask, 5u << 16);
    for (uint32_t i = 2; i < 8u; ++i) {
        if (memory.ReadByte(0x2000u + i) != i - 1u) {
            std::printf("reverse mask copy failed at %u\n", i); return 1;
        }
    }
    uint32_t alpha1_copy[0x100] = {};
    alpha1_copy[0] = 0x1000u; alpha1_copy[1] = 0x4000u;
    alpha1_copy[4] = 0x2000u; alpha1_copy[5] = 0u;
    alpha1_copy[8] = 0x800u; alpha1_copy[9] = 0u;
    alpha1_copy[0xA] = 0xFFFFFFFFu; alpha1_copy[0xB] = 0u;
    alpha1_copy[0xC] = 0x8900u; alpha1_copy[0xD] = 0x404u;
    alpha1_copy[0xE] = 4u; alpha1_copy[0xF] = 2u;
    alpha1_copy[0x10] = 0xFFFFFFu; alpha1_copy[0x11] = 0x140u;
    alpha1_copy[0xF1] = 0x20001u;
    memory.WriteWord(0x2000u, 2u); memory.WriteWord(0x1000u, 0u);
    direct.Copy(alpha1_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xFFFF0FFFu) {
        std::printf("one-bit alpha source expansion failed: %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    uint32_t mask_copy[0x100] = {};
    mask_copy[0] = 0x1000u; mask_copy[1] = 0xE000u;
    mask_copy[2] = 0x2000u; mask_copy[3] = 0x20D000u;
    mask_copy[8] = 0x800u; mask_copy[9] = 0u;
    mask_copy[0xC] = 0x900u; mask_copy[0xD] = 0x404u; mask_copy[0xE] = 2u;
    mask_copy[0xF] = 2u; mask_copy[0x10] = 0xFFFFFFu; mask_copy[0x11] = 0x20u;
    mask_copy[0x14] = mask_copy[0x18] = 0x12010u; mask_copy[0xF1] = 0x20001u;
    memory.WriteWord(0x2000u, 0x11223340u); memory.WriteWord(0x2004u, 0x556677C0u);
    memory.WriteWord(0x1000u, 0xAABBCCDDu);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xAABBC040u) {
        std::printf("mask alpha copy produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    mask_copy[3] = 0x22D000u; memory.WriteWord(0x1000u, 0u);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0x0000C040u) {
        std::printf("sRGB mask alpha copy failed\n"); return 1;
    }
    mask_copy[3] = 0x20D000u;
    mask_copy[0x11] = 0x40u; memory.WriteWord(0x1000u, 0u); direct.Copy(mask_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0x0000C040u) {
        std::printf("unblended mask alpha copy failed\n"); return 1;
    }
    mask_copy[0xE] = 3u; mask_copy[0x11] = 0x29u; mask_copy[0xF1] = 0x10001u;
    mask_copy[0x14] = mask_copy[0x18] = 0x000A3010u;
    mask_copy[0x15] = mask_copy[0x19] = 0x00203010u;
    memory.WriteWord(0x2000u, 0x11223340u); memory.WriteWord(0x1000u, 0xAABBCC80u);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xAABBCCA0u) {
        std::printf("mask union produced %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    mask_copy[3] = 0x206000u; mask_copy[0xE] = 2u; mask_copy[0x11] = 0x20u;
    mask_copy[0x14] = mask_copy[0x18] = 0x12010u;
    memory.WriteHalf(0x2000u, 0xF800u); memory.WriteWord(0x1000u, 0u);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadByte(0x1000u) != 255u) {
        std::printf("opaque RGB565 mask copy failed\n"); return 1;
    }
    mask_copy[3] = 0x20E000u; memory.WriteWord(0x2000u, 0x00000059u);
    memory.WriteWord(0x1000u, 0u); direct.Copy(mask_copy, 0u);
    if (memory.ReadByte(0x1000u) != 0x59u) {
        std::printf("8-bit mask copy failed\n"); return 1;
    }
    mask_copy[3] = 0x203000u; memory.WriteWord(0x1000u, 0u); direct.Copy(mask_copy, 0u);
    if (memory.ReadByte(0x1000u) != 255u) {
        std::printf("luminance mask source should be opaque\n"); return 1;
    }
    mask_copy[4] = 0x2000u; mask_copy[5] = 0x20E000u;
    mask_copy[0xE] = 4u; mask_copy[0xC] = 0x8900u; mask_copy[0x11] = 0x120u;
    memory.WriteWord(0x2000u, 0x00000073u); memory.WriteWord(0x1000u, 0u);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadByte(0x1000u) != 0x73u) {
        std::printf("second mask source copy failed\n"); return 1;
    }
    mask_copy[5] = 0x2000u; mask_copy[8] = 0x800u; mask_copy[0xF1] = 0x20001u;
    memory.WriteWord(0x2000u, 0x000000E2u); memory.WriteWord(0x1000u, 0u);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0x0000EE22u) {
        std::printf("four-bit second source copy failed\n"); return 1;
    }
    mask_copy[0xE] = 2u; mask_copy[0xC] = 0x900u; mask_copy[0x11] = 0x20u;
    mask_copy[3] = 0x201000u; memory.WriteWord(0x2000u, 0u); memory.WriteWord(0x1000u, 0u);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadByte(0x1000u) != 255u) {
        std::printf("one-bit black-and-white mask alpha failed\n"); return 1;
    }
    mask_copy[3] = 0x202000u; mask_copy[8] = 0x800u; mask_copy[0xF1] = 0x20001u;
    memory.WriteWord(0x2000u, 0x000000F3u); memory.WriteWord(0x1000u, 0u);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0x0000FF33u) {
        std::printf("four-bit mask alpha copy failed\n"); return 1;
    }
    uint32_t bw_copy[0x100] = {};
    bw_copy[0] = 0x1000u; bw_copy[1] = 0x7001u;
    bw_copy[2] = 0x2000u; bw_copy[3] = 0x1000u;
    bw_copy[8] = 0x800u; bw_copy[9] = 0u; bw_copy[0xD] = 0x404u;
    bw_copy[0xE] = 2u; bw_copy[0xF] = 2u; bw_copy[0x10] = 0xFFFFFFu;
    bw_copy[0xF1] = 0x20001u; memory.WriteWord(0x2000u, 2u);
    direct.Copy(bw_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xFF000000u || memory.ReadWord(0x1004u) != 0xFFFFFFFFu) {
        std::printf("black-and-white image copy failed\n"); return 1;
    }
    bw_copy[3] = 0u;
    bw_copy[0xA] = 0xFF123456u; bw_copy[0xB] = 0xFFABCDEFu;
    memory.WriteWord(0x2000u, 2u);
    direct.Copy(bw_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xFFABCDEFu || memory.ReadWord(0x1004u) != 0xFF123456u) {
        std::printf("one-bit foreground/background image copy failed\n"); return 1;
    }
    mask_copy[1] = 0xE000u; mask_copy[3] = 0x20E000u;
    mask_copy[8] = 0u; mask_copy[0xF1] = 0x10001u;
    memory.WriteWord(0x2000u, 0x0000006Bu); memory.WriteWord(0x1000u, 0u);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadByte(0x1000u) != 0x6Bu) {
        std::printf("A8 destination copy failed\n"); return 1;
    }
    mask_copy[1] = 0x3000u; mask_copy[3] = 0x1000u;
    mask_copy[8] = 0x800u; mask_copy[0xF1] = 0x20001u;
    memory.WriteWord(0x2000u, 2u); memory.WriteWord(0x1000u, 0xAABBCCDDu);
    direct.Copy(mask_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xAABBFF00u) {
        std::printf("black-and-white to G2D_8 mask copy failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    mask_copy[3] = 0x207000u; memory.WriteWord(0x2000u, 0xFFCC2233u);
    memory.WriteWord(0x1000u, 0u); mask_copy[0xF1] = 0x10001u;
    direct.Copy(mask_copy, 0u);
    if (memory.ReadByte(0x1000u) != 0xCCu) {
        std::printf("G2D_8 mask red/blue swap failed: %02x\n", memory.ReadByte(0x1000u)); return 1;
    }
    mask_copy[0xF1] = 0x20001u;
    mask_copy[3] = 0x2000u; memory.WriteWord(0x2000u, 0xF3u);
    memory.WriteWord(0x1000u, 0u); direct.Copy(mask_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0x0000FF33u) {
        std::printf("four-bit to G2D_8 mask copy failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    mask_copy[1] = 0xE000u; mask_copy[3] = 0x20E000u;
    mask_copy[4] = 0x2000u; mask_copy[5] = 0u;
    mask_copy[8] = 0x800u; mask_copy[0xF1] = 0x20001u;
    mask_copy[0xA] = 0xD0000000u; mask_copy[0xB] = 0x30000000u;
    mask_copy[0xE] = 4u; mask_copy[0xC] = 0x8100u;
    memory.WriteWord(0x2000u, 2u); direct.Copy(mask_copy, 0u);
    if (memory.ReadByte(0x1000u) != 0x30u || memory.ReadByte(0x1001u) != 0xD0u) {
        std::printf("one-bit second-source mask copy failed\n"); return 1;
    }
    bw_copy[3] = 0x26000u; memory.WriteHalf(0x2000u, 0xF800u);
    memory.WriteHalf(0x2002u, 0x07E0u); direct.Copy(bw_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xFFFF0000u || memory.ReadWord(0x1004u) != 0xFF00FF00u) {
        std::printf("RGB565 image copy failed\n"); return 1;
    }
    bw_copy[3] = 0x20D000u;
    memory.WriteWord(0x2000u, 0x123456FFu); memory.WriteWord(0x2004u, 0x789ABCFFu);
    direct.Copy(bw_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xFF563412u || memory.ReadWord(0x1004u) != 0xFFBC9A78u) {
        std::printf("RGBA source red/blue swap failed\n"); return 1;
    }
    bw_copy[3] = 0x3000u; memory.WriteWord(0x2000u, 0x0000C040u); direct.Copy(bw_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xFF404040u || memory.ReadWord(0x1004u) != 0xFFC0C0C0u) {
        std::printf("luminance image copy failed\n"); return 1;
    }
    bw_copy[3] = 0xE000u; direct.Copy(bw_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0x40FFFFFFu || memory.ReadWord(0x1004u) != 0xC0FFFFFFu) {
        std::printf("alpha image copy failed\n"); return 1;
    }
    bw_copy[4] = 0x2000u; bw_copy[5] = 0x2000u; bw_copy[0xE] = 4u;
    bw_copy[0xC] = 0x8100u; bw_copy[0x11] = 0x140u;
    memory.WriteWord(0x2000u, 0x000000E2u); direct.Copy(bw_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0x22FFFFFFu || memory.ReadWord(0x1004u) != 0xEEFFFFFFu) {
        std::printf("four-bit alpha-to-RGBA copy failed\n"); return 1;
    }
    bw_copy[5] = 0x2000u;
    memory.WriteWord(0x2004u, 0x00000E20u);
    direct.Copy(bw_copy, 0x10001u);
    if (memory.ReadWord(0x1000u) != 0x22FFFFFFu || memory.ReadWord(0x1004u) != 0xEEFFFFFFu) {
        std::printf("offset four-bit alpha-to-RGBA copy failed\n"); return 1;
    }
    bw_copy[5] = 0x2000u;
    bw_copy[0x11] = 0x120u; bw_copy[0x14] = 0x12010u; bw_copy[0x18] = 0x90110u;
    direct.Copy(bw_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0x22222222u || memory.ReadWord(0x1004u) != 0xEEEEEEEEu) {
        std::printf("premultiplied four-bit alpha copy failed\n"); return 1;
    }
    bw_copy[0x14] = 0x3010u; bw_copy[0x18] = 0x12010u; direct.Copy(bw_copy, 0u);
    if (memory.ReadWord(0x1000u) != 0xFFFFFFFFu || memory.ReadWord(0x1004u) != 0xFFFFFFFFu) {
        std::printf("opaque alpha-only source copy failed\n"); return 1;
    }
    memory.WriteWord(0x2000u, 0xFF000080u);
    uint32_t image_regs[0x100] = {};
    image_regs[0] = 0x1000u; image_regs[1] = 0x7000u;
    image_regs[0xD] = 0x404u; image_regs[0xE] = 1u; image_regs[0xF] = 8u;
    image_regs[0x10] = 0xFFFFFFu; image_regs[0x11] = 0x60u;
    image_regs[0x14] = image_regs[0x18] = 0x00812004u;
    image_regs[0xD0] = 0x41u; image_regs[0xD1] = 0x1080D000u;
    image_regs[0xD2] = 0x801u; image_regs[0xD3] = 0x2000u;
    image_regs[0xE0] = 0x10080632u; image_regs[0xE1] = 0x12098695u;
    image_regs[0xF1] = 0x10001u;
    cerfcore::Imx51Gpu2dImagePaint image_paint(emulator);
    for (uint32_t format : {5u, 12u}) {
        for (uint32_t alpha : {0x7fu, 0x80u}) {
            for (uint32_t straight : {0u, 1u}) {
                image_regs[1] = format << 12;
                image_regs[0x11] = straight ? 0x60u : 0x20u;
                memory.WriteWord(0x1000u, 0xBEEF0000u);
                memory.WriteWord(0x2000u, 0xFF000000u | alpha);
                image_paint.Composite(image_regs);
                const uint16_t pixel = memory.ReadHalf(0x1000u);
                const uint16_t alpha_bit = format == 5u ? 0x8000u : 1u;
                if ((pixel & alpha_bit) != (alpha >= 0x80u ? alpha_bit : 0u) ||
                    (!straight && alpha < 0x80u && pixel != 0u) ||
                    (straight && alpha < 0x80u && (pixel & ~alpha_bit) == 0u) ||
                    memory.ReadHalf(0x1002u) != 0xBEEFu) {
                    std::printf("1555 image output alpha mismatch: format=%u alpha=%u straight=%u pixel=%04x\n", format, alpha, straight, pixel);
                    return 1;
                }
            }
        }
    }
    image_regs[1] = 0x7000u;
    image_regs[0x11] = 0x60u;
    memory.WriteWord(0x2000u, 0x11223301u);
    image_paint.Composite(image_regs);
    if (memory.ReadWord(0x1000u) != 0x01112233u) {
        std::printf("straight-alpha image source lost precision: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    memory.WriteWord(0x2000u, 0x11223300u);
    image_paint.Composite(image_regs);
    if (memory.ReadWord(0x1000u) != 0u) {
        std::printf("transparent image source was not cleared\n"); return 1;
    }
    memory.WriteWord(0x2000u, 0xFF000080u);
    image_paint.Composite(image_regs);
    if (memory.ReadWord(0x1000u) != 0x80FF0000u) {
        std::printf("normal image draw produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    image_regs[1] = 0x4000u; memory.WriteWord(0x1000u, 0xBEEF0000u);
    image_paint.Composite(image_regs);
    if (memory.ReadWord(0x1000u) != 0xBEEF8F00u) {
        std::printf("ARGB4444 image composite failed: %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    image_regs[1] = 0x7000u;
    image_regs[0xE] = 5u; image_regs[4] = 0x2008u; image_regs[5] = 0x3000u;
    memory.WriteByte(0x2008u, 0x80u); memory.WriteWord(0x1000u, 0u);
    image_paint.Composite(image_regs);
    if (memory.ReadWord(0x1000u) != 0x40FF0000u) {
        std::printf("partially masked image produced %08x\n", memory.ReadWord(0x1000u)); return 1;
    }
    memory.WriteByte(0x2008u, 0u); memory.WriteWord(0x1000u, 0xFF00FF00u);
    image_paint.Composite(image_regs);
    if (memory.ReadWord(0x1000u) != 0xFF00FF00u) {
        std::printf("transparent image mask changed destination\n"); return 1;
    }
    image_regs[0xE] = 1u;
    image_regs[0xF] = 9u; image_regs[0xD0] = 0x188u;
    image_regs[0x11] = 0x69u; image_regs[0xC] = 0x4100u;
    image_regs[0x14] = 0x00190320u; image_regs[0x18] = 0x00190020u;
    image_regs[0x15] = image_regs[0x19] = 0x00852004u;
    image_regs[0xFF] = 0xFFFFFFFFu;
    image_paint.Composite(image_regs);
    if (memory.ReadWord(0x1000u) != 0x80FF0000u) {
        std::printf("RGBA image paint produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    image_regs[0xD1] = 0x10807000u;
    memory.WriteWord(0x2000u, 0x80FF0000u);
    image_paint.Composite(image_regs);
    if (memory.ReadWord(0x1000u) != 0x80FF0000u) {
        std::printf("image paint requiring premultiplication produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    image_regs[0xD1] = 0x10007000u;
    memory.WriteWord(0x2000u, 0x80800000u);
    image_paint.Composite(image_regs);
    if (memory.ReadWord(0x1000u) != 0x80FF0000u) {
        std::printf("premultiplied image paint produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    uint32_t paint_regs[0x100] = {};
    paint_regs[0xD0] = 0x40u; paint_regs[0xD1] = 0x7000u;
    paint_regs[0xD2] = 0x801u; paint_regs[0xD3] = 0x2004u;
    paint_regs[0xE0] = 0x10080632u;
    memory.WriteWord(0x2004u, 0xFF0000FFu);
    memory.WriteWord(0x2000u, 0x80808080u);
    image_regs[0xF] = 8u; image_regs[0xD0] = 0x1C8u;
    image_regs[0xD1] = 0x10007000u;
    memory.WriteWord(0x1000u, 0u);
    image_paint.Composite(image_regs, paint_regs);
    if (memory.ReadWord(0x1000u) != 0x800000FFu) {
        std::printf("two-texture blend produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    paint_regs[0xD1] |= 1u << 27;
    memory.WriteWord(0x1000u, 0u);
    image_paint.Composite(image_regs, paint_regs);
    if (memory.ReadWord(0x1000u) != 0x80FF0000u) {
        std::printf("two-texture paint channel swap produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    paint_regs[0xD1] &= ~(1u << 27);
    cerfcore::Gpu2dFillTarget target{};
    target.dest_pa = 0x1000u; target.stride_dw = 1;
    target.clip_l = target.clip_r = target.clip_t = target.clip_b = 0;
    target.argb = 0xFFFFFFFFu; target.blend = true;
    target.prog_a = target.prog_c = 0x00812004u;
    cerfcore::Imx51Gpu2dRasterizer rasterizer(emulator);
    rasterizer.OnReady();
    for (uint32_t format : {5u, 12u}) {
        for (uint32_t alpha : {0x7fu, 0x80u}) {
            for (uint32_t straight : {0u, 1u}) {
                cerfcore::Gpu2dFillTarget alpha_raster{};
                alpha_raster.dest_pa = 0x1000u; alpha_raster.stride_dw = 1;
                alpha_raster.clip_l = alpha_raster.clip_r = alpha_raster.clip_t = alpha_raster.clip_b = 0;
                alpha_raster.dest_argb1555 = format == 5u; alpha_raster.dest_5551 = format == 12u;
                alpha_raster.argb = (alpha << 24) | 0x00800000u; alpha_raster.blend = true;
                alpha_raster.oo_alpha = straight != 0u;
                alpha_raster.prog_a = alpha_raster.prog_c = 0x00812004u;
                memory.WriteWord(0x1000u, 0xBEEF0000u);
                rasterizer.FillContours(alpha_raster, {{0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f}}, 0u, true);
                const uint16_t pixel = memory.ReadHalf(0x1000u);
                const uint16_t alpha_bit = format == 5u ? 0x8000u : 1u;
                if ((pixel & alpha_bit) != (alpha >= 0x80u ? alpha_bit : 0u) ||
                    (!straight && alpha < 0x80u && pixel != 0u) ||
                    (straight && alpha < 0x80u && (pixel & ~alpha_bit) == 0u) ||
                    memory.ReadHalf(0x1002u) != 0xBEEFu) {
                    std::printf("1555 raster output mismatch: format=%u alpha=%u straight=%u pixel=%04x\n", format, alpha, straight, pixel);
                    return 1;
                }
            }
        }
    }
    for (uint32_t format : {5u, 12u}) {
        for (uint32_t straight : {0u, 1u}) {
            cerfcore::Gpu2dFillTarget covered{};
            covered.dest_pa = 0x1000u; covered.stride_dw = 1;
            covered.clip_l = covered.clip_r = covered.clip_t = covered.clip_b = 0;
            covered.dest_argb1555 = format == 5u; covered.dest_5551 = format == 12u;
            covered.argb = 0x80800000u; covered.blend = true; covered.antialias = true;
            covered.oo_alpha = straight != 0u;
            covered.prog_a = covered.prog_c = 0x00812004u;
            memory.WriteWord(0x1000u, 0xBEEF0000u);
            rasterizer.FillContours(covered, {{0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 1.0f}}, 0u, true);
            const uint16_t pixel = memory.ReadHalf(0x1000u);
            if ((!straight && pixel != 0u) || (straight && pixel == 0u) ||
                memory.ReadHalf(0x1002u) != 0xBEEFu) {
                std::printf("1555 partial coverage mismatch: format=%u straight=%u pixel=%04x\n", format, straight, pixel);
                return 1;
            }
        }
    }
    const char* stroke_state_path = "gpu2d-stroke-state-test.img";
    rasterizer.MoveTo(0.0f, 0.0f, false);
    rasterizer.LineTo(1.0f, 0.0f, true); rasterizer.LineTo(2.0f, 0.0f);
    {
        cerfcore::StateWriter writer(emulator, stroke_state_path);
        rasterizer.SaveState(writer); rasterizer.SaveSmoothState(writer);
        if (!writer.Commit()) { std::printf("stroke state save failed\n"); return 1; }
    }
    cerfcore::Imx51Gpu2dRasterizer restored_rasterizer(emulator);
    {
        cerfcore::StateReader reader(emulator, stroke_state_path);
        restored_rasterizer.RestoreState(reader); restored_rasterizer.RestoreSmoothState(reader);
        if (!reader.Ok() || restored_rasterizer.PathSmooth() != std::vector<uint8_t>({0u, 1u, 0u})) {
            std::printf("stroke subdivision state restore failed\n"); return 1;
        }
    }
    {
        cerfcore::StateWriter writer(emulator, stroke_state_path);
        rasterizer.SaveState(writer);
        if (!writer.Commit()) { std::printf("legacy stroke state save failed\n"); return 1; }
    }
    {
        cerfcore::StateReader reader(emulator, stroke_state_path);
        restored_rasterizer.RestoreState(reader); restored_rasterizer.RestoreSmoothState(reader);
        if (!reader.Ok() || restored_rasterizer.PathSmooth() != std::vector<uint8_t>({0u, 0u, 0u})) {
            std::printf("legacy stroke state restore failed\n"); return 1;
        }
    }
    std::remove(stroke_state_path); rasterizer.ClearPath();
    cerfcore::Gpu2dFillTarget binary_contour{};
    binary_contour.dest_pa = 0x1000u; binary_contour.stride_dw = 1;
    binary_contour.clip_l = binary_contour.clip_t = binary_contour.clip_r = binary_contour.clip_b = 0;
    binary_contour.argb = 0xFFFFFFFFu; binary_contour.antialias = false;
    memory.WriteWord(0x1000u, 0u);
    rasterizer.FillContours(binary_contour, {{0.0f, 0.0f, 1.25f, 0.0f, 0.0f, 1.25f}},
                            binary_contour.argb, false);
    if (memory.ReadWord(0x1000u) != 0xFFFFFFFFu) {
        std::printf("non-antialiased contour did not use binary coverage\n"); return 1;
    }
    // A crossing just beyond the pixel center rounds back to the center.
    memory.WriteWord(0x1000u, 0u);
    rasterizer.FillContours(binary_contour, {{0.51f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.51f, 1.0f}},
                            binary_contour.argb, false);
    if (memory.ReadWord(0x1000u) != 0xFFFFFFFFu) {
        std::printf("sampled contour crossing did not use subpixel precision\n"); return 1;
    }
    cerfcore::Gpu2dFillTarget alpha_target{};
    alpha_target.dest_pa = 0x2000u; alpha_target.stride_dw = 2; alpha_target.dest_a8 = true;
    alpha_target.clip_l = 0; alpha_target.clip_r = 3;
    alpha_target.clip_t = 0; alpha_target.clip_b = 1;
    alpha_target.argb = 0x5A123456u;
    memory.WriteWord(0x2000u, 0u); memory.WriteWord(0x2008u, 0u);
    rasterizer.FillRect(alpha_target, 1, 0, 2, 2);
    if (memory.ReadWord(0x2000u) != 0x005A5A00u || memory.ReadWord(0x2008u) != 0x005A5A00u) {
        std::printf("A8 fill did not preserve adjacent pixels or row stride\n");
        return 1;
    }
    cerfcore::Gpu2dFillTarget rgba16_target{};
    rgba16_target.dest_pa = 0x2000u; rgba16_target.stride_dw = 1; rgba16_target.dest_5551 = true;
    rgba16_target.clip_l = rgba16_target.clip_t = rgba16_target.clip_b = 0; rgba16_target.clip_r = 1;
    rgba16_target.argb = 0xFFFF0000u; memory.WriteWord(0x2000u, 0u);
    rasterizer.FillRect(rgba16_target, 0, 0, 1, 1);
    rgba16_target.argb = 0x0000FF00u; rasterizer.FillRect(rgba16_target, 1, 0, 1, 1);
    if (memory.ReadWord(0x2000u) != 0x07C0F801u) {
        std::printf("RGBA5551 fill produced %08x\n", memory.ReadWord(0x2000u)); return 1;
    }
    rgba16_target.dest_5551 = false; rgba16_target.dest_4444 = true;
    rgba16_target.argb = 0xFFFF0000u; memory.WriteWord(0x2000u, 0u);
    rasterizer.FillRect(rgba16_target, 0, 0, 1, 1);
    rgba16_target.argb = 0x0000FF00u; rasterizer.FillRect(rgba16_target, 1, 0, 1, 1);
    if (memory.ReadWord(0x2000u) != 0x0F00F00Fu) {
        std::printf("RGBA4444 fill produced %08x\n", memory.ReadWord(0x2000u)); return 1;
    }
    alpha_target.blend = true; alpha_target.antialias = false;
    alpha_target.prog_a = alpha_target.prog_c = 0x00812004u;
    memory.WriteWord(0x2000u, 0u);
    rasterizer.MoveTo(1.0f, 0.0f, true);
    rasterizer.LineTo(3.0f, 0.0f); rasterizer.LineTo(3.0f, 1.0f); rasterizer.LineTo(1.0f, 1.0f);
    rasterizer.Flush(alpha_target);
    if (memory.ReadWord(0x2000u) != 0x005A5A00u) {
        std::printf("A8 vector fill produced %08x\n", memory.ReadWord(0x2000u)); return 1;
    }
    cerfcore::Gpu2dFillTarget msaa_target{};
    msaa_target.dest_pa = 0x1000u; msaa_target.stride_dw = 1;
    msaa_target.clip_l = msaa_target.clip_t = msaa_target.clip_r = msaa_target.clip_b = 0;
    msaa_target.argb = 0xFFFF0000u; msaa_target.multisample = true;
    for (uint32_t lane = 0; lane < 4; ++lane) memory.WriteWord(0x1000u + lane * 4u, 0u);
    auto draw_msaa_rect = [&](float left, float right) {
        rasterizer.MoveTo(left, 0.0f, true);
        rasterizer.LineTo(right, 0.0f); rasterizer.LineTo(right, 1.0f);
        rasterizer.LineTo(left, 1.0f); rasterizer.Flush(msaa_target);
    };
    draw_msaa_rect(0.0f, 0.5f);
    for (uint32_t lane = 0; lane < 4; ++lane) {
        const uint32_t expected = (lane == 0 || lane == 2) ? 0xFFFF0000u : 0u;
        if (memory.ReadWord(0x1000u + lane * 4u) != expected) {
            std::printf("multisample geometry selected wrong lane %u\n", lane); return 1;
        }
    }
    draw_msaa_rect(0.5f, 1.0f);
    for (uint32_t lane = 0; lane < 4; ++lane)
        if (memory.ReadWord(0x1000u + lane * 4u) != 0xFFFF0000u) {
            std::printf("complementary multisample fills left lane %u uncovered\n", lane); return 1;
        }
    for (uint32_t lane = 0; lane < 4; ++lane) {
        memory.WriteWord(0x1000u + lane * 4u, 0u);
        memory.WriteWord(0x2000u + lane * 4u, (lane == 0 || lane == 2) ? 255u : 0u);
    }
    msaa_target.mask_pa = 0x2000u; msaa_target.mask_stride_dw = 1;
    msaa_target.blend = true; msaa_target.prog_a = msaa_target.prog_c = 0x00812004u;
    draw_msaa_rect(0.0f, 1.0f);
    for (uint32_t lane = 0; lane < 4; ++lane) {
        const uint32_t expected = (lane == 0 || lane == 2) ? 0xFFFF0000u : 0u;
        if (memory.ReadWord(0x1000u + lane * 4u) != expected) {
            std::printf("multisample mask selected wrong lane %u\n", lane); return 1;
        }
    }
    msaa_target.mask_pa = 0; msaa_target.blend = false;
    msaa_target.sample_scissor_top = 1; msaa_target.sample_scissor_bottom = 2;
    for (uint32_t lane = 0; lane < 4; ++lane) memory.WriteWord(0x1000u + lane * 4u, 0u);
    draw_msaa_rect(0.0f, 1.0f);
    for (uint32_t lane = 0; lane < 4; ++lane) {
        const uint32_t expected = (lane == 1 || lane == 2) ? 0xFFFF0000u : 0u;
        if (memory.ReadWord(0x1000u + lane * 4u) != expected) {
            std::printf("multisample scissor selected wrong lane %u\n", lane); return 1;
        }
    }
    msaa_target.sample_scissor_top = 0; msaa_target.sample_scissor_bottom = 0x7fffffff;
    cerfcore::Gpu2dGradwPaint sample_paint{};
    sample_paint.texbase = 0x2000u; sample_paint.texw = 1; sample_paint.texh = 4;
    sample_paint.tstride = 1; sample_paint.format = 7u; sample_paint.premultiply = true;
    sample_paint.ninst = 2; sample_paint.inst[0] = 0x10080632u;
    sample_paint.inst[1] = 0x12098695u; sample_paint.cnst[4] = 0.25f;
    for (uint32_t lane = 0; lane < 4; ++lane)
        memory.WriteWord(0x2000u + lane * 4u, 0xFF000000u | (lane * 0x00303030u));
    msaa_target.mask_pa = 0; msaa_target.blend = false; msaa_target.paint = &sample_paint;
    draw_msaa_rect(0.0f, 1.0f);
    for (uint32_t lane = 0; lane < 4; ++lane) {
        const uint32_t expected = 0xFF000000u;
        if (memory.ReadWord(0x1000u + lane * 4u) != expected) {
            std::printf("multisample paint did not use the shared pixel coordinate for lane %u\n", lane); return 1;
        }
    }
    for (uint32_t lane = 0; lane < 4; ++lane) {
        memory.WriteWord(0x2000u + lane * 4u, 0x01112233u);
        memory.WriteWord(0x1000u + lane * 4u, 0u);
    }
    sample_paint.premultiply = false;
    msaa_target.blend = true; msaa_target.oo_alpha = true;
    msaa_target.prog_a = msaa_target.prog_c = 0x00812004u;
    draw_msaa_rect(0.0f, 1.0f);
    for (uint32_t lane = 0; lane < 4; ++lane)
        if (memory.ReadWord(0x1000u + lane * 4u) != 0x01112233u) {
            std::printf("multisample source replacement lost precision at lane %u\n", lane); return 1;
        }
    cerfcore::Gpu2dFillTarget packed_blend{};
    packed_blend.dest_pa = 0x1000u; packed_blend.stride_dw = 1;
    packed_blend.clip_l = packed_blend.clip_t = packed_blend.clip_r = packed_blend.clip_b = 0;
    packed_blend.dest_argb4444 = true; packed_blend.blend = true;
    packed_blend.prog_a = packed_blend.prog_c = 0x00822004u;
    memory.WriteWord(0x1000u, 0xBEEF1234u);
    rasterizer.MoveTo(0.0f, 0.0f, true); rasterizer.LineTo(1.0f, 0.0f);
    rasterizer.LineTo(1.0f, 1.0f); rasterizer.LineTo(0.0f, 1.0f);
    rasterizer.Flush(packed_blend);
    if (memory.ReadWord(0x1000u) != 0xBEEF1234u) {
        std::printf("ARGB4444 blend did not preserve destination: %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    auto draw_fractional = [&](bool antialias) {
        target.antialias = antialias;
        memory.WriteWord(0x1000u, 0xFF000000u);

        rasterizer.MoveTo(0.25f, 0.25f, true);
        rasterizer.LineTo(1.25f, 0.25f);
        rasterizer.LineTo(1.25f, 1.25f);
        rasterizer.LineTo(0.25f, 1.25f);

        rasterizer.Flush(target);
        return memory.ReadWord(0x1000u);
    };
    if (draw_fractional(false) != 0xFFFFFFFFu || draw_fractional(true) != 0xFF8F8F8Fu) {
        std::printf("fractional coverage did not honor antialias mode\n");
        return 1;
    }
    memory.WriteWord(0x2000u, 0x000000FFu);
    memory.WriteWord(0x1000u, 0u);
    target.mask_pa = 0x2000u; target.mask_stride_dw = 1;
    target.antialias = false;
    rasterizer.MoveTo(0.0f, 0.0f, true);
    rasterizer.LineTo(1.0f, 0.0f); rasterizer.LineTo(1.0f, 1.0f); rasterizer.LineTo(0.0f, 1.0f);
    rasterizer.Flush(target);
    if (memory.ReadWord(0x1000u) != 0xFFFFFFFFu) {
        std::printf("opaque mask rejected fill\n"); return 1;
    }
    memory.WriteWord(0x2000u, 0u); memory.WriteWord(0x1000u, 0u);
    rasterizer.MoveTo(0.0f, 0.0f, true);
    rasterizer.LineTo(1.0f, 0.0f); rasterizer.LineTo(1.0f, 1.0f); rasterizer.LineTo(0.0f, 1.0f);
    rasterizer.Flush(target);
    if (memory.ReadWord(0x1000u) != 0u) {
        std::printf("transparent mask changed destination\n"); return 1;
    }
    target.mask_pa = 0u;
    cerfcore::Gpu2dGradwPaint vector_image{}, vector_paint{};
    vector_image.texbase = 0x2000u; vector_paint.texbase = 0x2004u;
    vector_image.texw = vector_image.texh = vector_paint.texw = vector_paint.texh = 1;
    vector_image.tstride = vector_paint.tstride = 1;
    vector_image.format = vector_paint.format = 7u;
    vector_image.premultiply = vector_paint.premultiply = true;
    vector_image.ninst = vector_paint.ninst = 1;
    vector_image.inst[0] = vector_paint.inst[0] = 0x10080632u;
    memory.WriteWord(0x2000u, 0x80808080u);
    memory.WriteWord(0x2004u, 0xFF0000FFu);
    memory.WriteWord(0x1000u, 0u);
    target.paint = &vector_image; target.second_paint = &vector_paint;
    target.generic_blend = true; target.oo_alpha = true; target.antialias = false;
    target.alpha_passes = target.color_passes = 2;
    target.alpha_program[0] = 0x00190320u; target.color_program[0] = 0x00190020u;
    target.alpha_program[1] = target.color_program[1] = 0x00852004u;
    rasterizer.MoveTo(0.0f, 0.0f, true);
    rasterizer.LineTo(1.0f, 0.0f); rasterizer.LineTo(1.0f, 1.0f); rasterizer.LineTo(0.0f, 1.0f);
    rasterizer.Flush(target);
    if (memory.ReadWord(0x1000u) != 0x800000FFu) {
        std::printf("two-texture vector blend produced %08x\n", memory.ReadWord(0x1000u));
        return 1;
    }
    auto& stroke_rasterizer = emulator.Get<cerfcore::Imx51Gpu2dRasterizer>();
    stroke_rasterizer.OnReady();
    cerfcore::Imx51Gpu2dStroker stroker(emulator);
    cerfcore::Gpu2dFillTarget stroke_target{};
    stroke_target.dest_pa = 0x3000u; stroke_target.stride_dw = 16;
    stroke_target.clip_l = stroke_target.clip_t = 0;
    stroke_target.clip_r = stroke_target.clip_b = 11;
    stroke_target.argb = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    uint32_t stroke_pixels[16u * 12u] = {};
    stroke_target.blend = true; stroke_target.prog_a = stroke_target.prog_c = 0x00812004u;
    for (int rule = 0; rule < 2; ++rule) {
        stroke_target.even_odd = rule != 0;
        for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
        stroke_rasterizer.MoveTo(3.0f, 5.0f, false);
        stroke_rasterizer.LineTo(6.0f, 5.0f); stroke_rasterizer.LineTo(6.0f, 8.0f);
        stroker.Stroke(stroke_target, 2.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 1u, 0.0f);
        for (uint32_t i = 0; i < 16u * 12u; ++i) {
            const uint32_t pixel = memory.ReadWord(0x3000u + i * 4u);
            if (rule == 0) stroke_pixels[i] = pixel;
            else if (pixel != stroke_pixels[i]) {
                std::printf("stroke changed with path fill rule at pixel %u\n", i); return 1;
            }
        }
    }
    stroke_target.blend = false; stroke_target.even_odd = false;
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_target.antialias = false;
    stroke_rasterizer.MoveTo(5.0f, 5.0f, false);
    stroke_rasterizer.LineTo(5.1f, 5.0f, true); stroke_rasterizer.LineTo(5.2f, 5.0f);
    stroker.Stroke(stroke_target, 3.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 1u, 0.0f);
    if (memory.ReadWord(0x3000u + (5u * 16u + 2u) * 4u) != 0u) {
        std::printf("smooth curve subdivision extended beyond butt cap\n"); return 1;
    }
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_rasterizer.MoveTo(5.0f, 5.0f, false);
    stroke_rasterizer.LineTo(5.1f, 5.0f); stroke_rasterizer.LineTo(5.2f, 5.01f);
    stroker.Stroke(stroke_target, 3.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 1u, 0.0f);
    if (memory.ReadWord(0x3000u + (5u * 16u + 2u) * 4u) != 0u) {
        std::printf("authored round join extended beyond butt cap\n"); return 1;
    }
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_rasterizer.MoveTo(5.0f, 5.0f, false);
    stroke_rasterizer.LineTo(6.0f, 5.0f, true); stroke_rasterizer.LineTo(5.0f, 5.0f);
    stroker.Stroke(stroke_target, 2.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 0u, 0.0f);
    if (memory.ReadWord(0x3000u + (5u * 16u + 7u) * 4u) != 0xFFFFFFFFu) {
        std::printf("smooth curve reversal lost cusp coverage\n"); return 1;
    }
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_rasterizer.MoveTo(4.0f, 5.0f, false);
    stroke_rasterizer.LineTo(5.0f, 5.0f, true); stroke_rasterizer.LineTo(4.004f, 5.089f);
    stroker.Stroke(stroke_target, 2.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 2u, 0.0f);
    if (memory.ReadWord(0x3000u + (3u * 16u + 10u) * 4u) != 0u) {
        std::printf("near-reversal generated an excessive miter\n"); return 1;
    }
    for (uint32_t cap = 0; cap <= 2; ++cap) {
        for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
        stroke_rasterizer.MoveTo(5.0f, 5.0f, true);
        stroker.Stroke(stroke_target, 2.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, cap, 2u, 0.0f);
        const uint32_t expected = cap == 0u ? 0u : 0xFFFFFFFFu;
        if (memory.ReadWord(0x3000u + (5u * 16u + 5u) * 4u) != expected) {
            std::printf("single-point closed stroke cap %u failed\n", cap); return 1;
        }
    }
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_rasterizer.MoveTo(5.0f, 6.0f, false);
    stroke_rasterizer.LineTo(4.5f, 6.25f, true); stroke_rasterizer.LineTo(4.25f, 5.0f, true);
    stroke_rasterizer.LineTo(4.5f, 3.75f, true); stroke_rasterizer.LineTo(5.0f, 4.0f);
    stroker.Stroke(stroke_target, 2.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 1u, 0.0f);
    if (memory.ReadWord(0x3000u + (6u * 16u + 6u) * 4u) != 0u) {
        std::printf("wide smooth curve extended beyond its butt caps\n"); return 1;
    }
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_rasterizer.MoveTo(5.0f, 2.0f, false);
    stroke_rasterizer.LineTo(5.0f, 5.5f, true); stroke_rasterizer.LineTo(5.3f, 6.0f);
    stroke_rasterizer.LineTo(5.5f, 6.01f, true); stroke_rasterizer.LineTo(5.6f, 6.2f, true);
    stroke_rasterizer.LineTo(5.65f, 6.6f, true); stroke_rasterizer.LineTo(5.65f, 7.0f);
    stroker.Stroke(stroke_target, 2.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 1u, 0.0f);
    if (memory.ReadWord(0x3000u + (8u * 16u + 5u) * 4u) != 0u) {
        std::printf("joined smooth curves extended beyond butt cap\n"); return 1;
    }
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_rasterizer.MoveTo(5.0f, 5.0f, false);
    stroke_rasterizer.LineTo(5.00005f, 5.0f, true);
    stroke_rasterizer.LineTo(5.00005f, 5.00005f, true);
    stroke_rasterizer.LineTo(5.0f, 5.0001f, true);
    stroke_rasterizer.LineTo(5.0001f, 5.0001f);
    stroker.Stroke(stroke_target, 3.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 1u, 0.0f);
    if (memory.ReadWord(0x3000u + (5u * 16u + 5u) * 4u) == 0u) {
        std::printf("small smooth stroke lost its geometry\n"); return 1;
    }
    uint32_t closed_stroke[16u * 12u] = {};
    for (int explicit_close = 0; explicit_close < 2; ++explicit_close) {
        for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
        stroke_rasterizer.MoveTo(4.0f, 4.0f, true);
        stroke_rasterizer.LineTo(8.0f, 4.0f); stroke_rasterizer.LineTo(6.0f, 7.0f);
        if (explicit_close) stroke_rasterizer.LineTo(4.0f, 4.0f);
        stroker.Stroke(stroke_target, 3.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 1u, 0.0f);
        for (uint32_t i = 0; i < 16u * 12u; ++i) {
            const uint32_t pixel = memory.ReadWord(0x3000u + i * 4u);
            if (!explicit_close) closed_stroke[i] = pixel;
            else if (closed_stroke[i] != pixel) {
                std::printf("explicitly closed stroke differs at pixel %u\n", i); return 1;
            }
        }
    }
    stroke_target.antialias = true;
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_rasterizer.MoveTo(4.0f, 4.0f, false);
    stroke_rasterizer.LineTo(4.0f, 8.0f);
    stroker.Stroke(stroke_target, 1.0f, 2.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 0u, 0.0f);
    if (memory.ReadWord(0x3000u + (5u * 16u + 2u) * 4u) != 0xFFFFFFFFu ||
        memory.ReadWord(0x3000u + (5u * 16u + 5u) * 4u) != 0xFFFFFFFFu ||
        memory.ReadWord(0x3000u + (5u * 16u + 1u) * 4u) != 0u ||
        memory.ReadWord(0x3000u + (5u * 16u + 6u) * 4u) != 0u) {
        std::printf("anisotropic stroke extent is wrong\n"); return 1;
    }
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_rasterizer.MoveTo(4.0f, 4.0f, false);
    stroke_rasterizer.LineTo(8.0f, 4.0f);
    stroker.Stroke(stroke_target, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0u, 0u, 0.0f);
    if (memory.ReadWord(0x3000u + (3u * 16u + 5u) * 4u) != 0xFFFFFFFFu ||
        memory.ReadWord(0x3000u + (4u * 16u + 5u) * 4u) != 0xFFFFFFFFu ||
        memory.ReadWord(0x3000u + (2u * 16u + 5u) * 4u) != 0u) {
        std::printf("rotated stroke extent is wrong\n"); return 1;
    }
    for (uint32_t i = 0; i < 16u * 12u; ++i) memory.WriteWord(0x3000u + i * 4u, 0u);
    stroke_rasterizer.MoveTo(4.0f, 4.0f, false);
    stroke_rasterizer.LineTo(8.0f, 4.0f);
    stroker.Stroke(stroke_target, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 2u, 0u, 0.0f);
    if (memory.ReadWord(0x3000u + (3u * 16u + 3u) * 4u) != 0xFFFFFFFFu ||
        memory.ReadWord(0x3000u + (3u * 16u + 8u) * 4u) != 0xFFFFFFFFu ||
        memory.ReadWord(0x3000u + (3u * 16u + 2u) * 4u) != 0u ||
        memory.ReadWord(0x3000u + (3u * 16u + 9u) * 4u) != 0u) {
        std::printf("square stroke cap extent is wrong\n"); return 1;
    }
    cerfcore::Gpu2dGradwPaint paint{};
    paint.texbase = 0x2000u; paint.texw = paint.texh = 2; paint.tstride = 2;
    paint.premultiply = true; paint.ninst = 2;
    paint.inst[0] = 0x10080632u; paint.inst[1] = 0x12098695u;
    paint.cnst[0] = paint.cnst[4] = 0.5f;
    paint.border_color = 0xFF445566u;
    memory.WriteWord(0x2000u, 0xFF102030u);
    memory.WriteWord(0x2004u, 0xFF405060u);
    memory.WriteWord(0x2008u, 0xFF708090u);
    memory.WriteWord(0x200Cu, 0xFFA0B0C0u);
    auto& sampler = emulator.Get<cerfcore::Imx51Gpu2dGradwSampler>();
    cerfcore::Gpu2dGradwPaint point_boundary = paint;
    point_boundary.cnst[0] = point_boundary.cnst[4] = 0.0f;
    point_boundary.cnst[2] = 0.5f - 0.000244140625f;
    if (sampler.Sample(point_boundary, 0, 0) != 0xFF405060u) {
        std::printf("point sampling near a texel boundary failed\n"); return 1;
    }
    point_boundary.cnst[2] = 0.5f - 0.002f;
    if (sampler.Sample(point_boundary, 0, 0) != 0xFF102030u) {
        std::printf("point sampling away from a texel boundary failed\n"); return 1;
    }
    point_boundary.cnst[2] = 0.0f;
    point_boundary.cnst[5] = 0.5f - 0.000244140625f;
    if (sampler.Sample(point_boundary, 0, 0) != 0xFF708090u) {
        std::printf("point sampling near a vertical texel boundary failed\n"); return 1;
    }
    point_boundary.cnst[5] = 0.0f;
    point_boundary.wrap_u = cerfcore::Gpu2dWrap::Repeat;
    point_boundary.cnst[2] = -0.01f;
    if (sampler.Sample(point_boundary, 0, 0) != 0xFF405060u) {
        std::printf("point sampling negative repeat coordinates failed\n"); return 1;
    }
    cerfcore::Gpu2dGradwPaint repeated_ramp = paint;
    repeated_ramp.texh = 1; repeated_ramp.ninst = 1; repeated_ramp.bilinear = true;
    repeated_ramp.wrap_u = cerfcore::Gpu2dWrap::Repeat;
    repeated_ramp.inst[0] = (8u << 25) | (28u << 20) | (28u << 15) |
        (28u << 10) | (28u << 5) | 28u;
    if (sampler.Sample(repeated_ramp, 0, 0) != 0xFF102030u) {
        std::printf("repeating gradient ramp seam failed\n"); return 1;
    }
    repeated_ramp.ninst = 2;
    repeated_ramp.inst[1] = (9u << 25) | (28u << 20) | (28u << 15) |
        (28u << 10) | (28u << 5) | 28u;
    if (sampler.Sample(repeated_ramp, 0, 0) != 0xFF102030u) {
        std::printf("one-row multi-instruction texture seam failed\n"); return 1;
    }
    paint.swap_rb = true;
    if (sampler.Sample(paint, 0, 0) != 0xFF302010u) {
        std::printf("texture red/blue swap failed\n"); return 1;
    }
    paint.swap_rb = false;
    paint.wrap_u = cerfcore::Gpu2dWrap::Repeat;
    stroke_target.paint = &paint; stroke_target.blend = true;
    stroke_target.prog_a = stroke_target.prog_c = 0x00812004u;
    stroke_rasterizer.MoveTo(4.0f, 4.0f, false);
    stroke_rasterizer.LineTo(8.0f, 4.0f);
    stroker.Stroke(stroke_target, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0u, 0u, 0.0f);
    if (memory.ReadWord(0x3000u + (3u * 16u + 4u) * 4u) != 0xFF708090u ||
        memory.ReadWord(0x3000u + (3u * 16u + 5u) * 4u) != 0xFFA0B0C0u) {
        std::printf("gradient-painted stroke failed\n"); return 1;
    }
    stroke_target.paint = nullptr; paint.wrap_u = cerfcore::Gpu2dWrap::Clamp;
    cerfcore::Gpu2dGradwPaint perspective = paint;
    perspective.ninst = 1;
    perspective.inst[0] = (1u << 29) | (8u << 25) | (16u << 20) | (17u << 15) |
        (28u << 10) | (28u << 5) | 28u;
    perspective.cnst[0] = 2.0f; perspective.cnst[1] = 0.5f;
    if (sampler.Sample(perspective, 0, 0) != 0xFF102030u) {
        std::printf("perspective reciprocal sampling failed\n"); return 1;
    }
    // Captured projective program computes both coordinates in its final RCP.
    perspective.ninst = 4;
    perspective.inst[0] = 0x04080632u; perspective.inst[1] = 0x06098695u;
    perspective.inst[2] = 0x080B06F8u; perspective.inst[3] = 0x3041707Cu;
    perspective.cnst[0] = 0.015625f; perspective.cnst[1] = 0.0f;
    perspective.cnst[2] = 0.00780869f; perspective.cnst[3] = -0.0078125f;
    perspective.cnst[4] = -0.03125f; perspective.cnst[5] = 1.98047f;
    perspective.cnst[6] = -0.015625f; perspective.cnst[7] = 0.0f;
    perspective.cnst[8] = 0.992188f;
    if (sampler.Sample(perspective, 0, 40) != 0xFF708090u) {
        std::printf("projective reciprocal vertical coordinate failed\n"); return 1;
    }
    cerfcore::Gpu2dGradwPaint radial = paint;
    radial.ninst = 1;
    radial.inst[0] = (3u << 29) | (8u << 25) | (16u << 20) | (28u << 15) |
        (17u << 10) | (28u << 5) | 28u;
    radial.cnst[0] = 0.5625f; radial.cnst[1] = -0.25f;
    if (sampler.Sample(radial, 0, 0) != 0xFF405060u) {
        std::printf("off-center radial square-root sampling failed\n"); return 1;
    }
    radial.cnst[0] = -0.00015f; radial.cnst[1] = 0.75f;
    if (sampler.Sample(radial, 0, 0) != 0xFF405060u) {
        std::printf("radial focal-point rounding failed\n"); return 1;
    }
    if (sampler.Sample(paint, -1, 0) != 0xFF102030u) {
        std::printf("clamp wrap failed\n"); return 1;
    }
    paint.wrap_u = cerfcore::Gpu2dWrap::Repeat;
    if (sampler.Sample(paint, -1, 0) != 0xFF405060u) {
        std::printf("repeat wrap failed\n"); return 1;
    }
    paint.wrap_u = cerfcore::Gpu2dWrap::Mirror;
    if (sampler.Sample(paint, -1, 0) != 0xFF102030u ||
        sampler.Sample(paint, 2, 0) != 0xFF405060u) {
        std::printf("mirror wrap failed\n"); return 1;
    }
    paint.wrap_u = cerfcore::Gpu2dWrap::Border;
    paint.wrap_v = cerfcore::Gpu2dWrap::Border;
    if (sampler.Sample(paint, -1, 0) != paint.border_color ||
        sampler.Sample(paint, 0, 2) != paint.border_color) {
        std::printf("border wrap failed\n"); return 1;
    }
    paint.format = 6u; paint.tstride = 1; paint.wrap_u = paint.wrap_v = cerfcore::Gpu2dWrap::Clamp;
    memory.WriteHalf(0x2000u, 0xF800u);
    memory.WriteHalf(0x2002u, 0x07E0u);
    if (sampler.Sample(paint, 0, 0) != 0xFFFF0000u ||
        sampler.Sample(paint, 1, 0) != 0xFF00FF00u) {
        std::printf("RGB565 texture sampling failed\n"); return 1;
    }
    paint.format = 11u;
    memory.WriteHalf(0x2000u, 0xF008u);
    if (sampler.Sample(paint, 0, 0) != 0x88FF0000u) {
        std::printf("RGBA4444 texture sampling failed\n"); return 1;
    }
    paint.format = 12u;
    memory.WriteHalf(0x2000u, 0x003Fu);
    if (sampler.Sample(paint, 0, 0) != 0xFF0000FFu) {
        std::printf("RGBA5551 texture sampling failed\n"); return 1;
    }
    paint.format = 3u; paint.tstride = 1; paint.premultiply = true;
    memory.WriteByte(0x2000u, 0x80u);
    if (sampler.Sample(paint, 0, 0) != 0xFF808080u) {
        std::printf("luminance texture sampling failed\n"); return 1;
    }
    paint.format = 14u; paint.premultiply = false;
    if (sampler.Sample(paint, 0, 0) != 0x80808080u) {
        std::printf("alpha texture sampling failed\n"); return 1;
    }
    paint.format = 7u; paint.tstride = 2; paint.srgb = true; paint.premultiply = true;
    memory.WriteWord(0x2000u, 0x7F4080C0u);
    if (sampler.Sample(paint, 0, 0) != 0x7F0D3786u) {
        std::printf("sRGB texture decoding failed\n"); return 1;
    }
    paint.format = 1u; paint.premultiply = true; paint.srgb = false;
    memory.WriteByte(0x2000u, 0x01u);
    if (sampler.Sample(paint, 0, 0) != 0xFFFFFFFFu ||
        sampler.Sample(paint, 1, 0) != 0xFF000000u) {
        std::printf("one-bit black-and-white sampling failed\n"); return 1;
    }
    paint.format = 0u; paint.foreground_color = 0xFF123456u;
    paint.background_color = 0xFFABCDEFu;
    if (sampler.Sample(paint, 0, 0) != 0xFF123456u ||
        sampler.Sample(paint, 1, 0) != 0xFFABCDEFu) {
        std::printf("one-bit foreground/background sampling failed\n"); return 1;
    }
    paint.format = 2u; paint.premultiply = false;
    memory.WriteByte(0x2000u, 0xF8u);
    if (sampler.Sample(paint, 0, 0) != 0x88888888u ||
        sampler.Sample(paint, 1, 0) != 0xFFFFFFFFu) {
        std::printf("four-bit alpha sampling failed\n"); return 1;
    }
    return 0;
}

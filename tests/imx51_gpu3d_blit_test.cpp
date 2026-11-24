#include "../src/socs/imx51/imx51_gpu3d_blit.h"
#include "../src/socs/imx51/imx51_gpu3d_memory.h"
#include "../src/boards/page_table_builder.h"
#include "../src/peripherals/peripheral_dispatcher.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/execution/jit_runner.h>
#include <bit>
#include <cstdio>
#include <memory>
#include <unordered_map>

class TestMemoryMap final : public cerfcore::PageTableBuilder {
public:
    using PageTableBuilder::PageTableBuilder;
    uint32_t VaToPa(uint32_t address) const override { return address; }
    std::vector<cerfcore::DramRegion> CachedDramRegions() const override { return {}; }
    std::vector<cerfcore::DramRegion> MappedVaSpans() const override { return {}; }
    std::vector<cerfcore::BackedRegion> BackedMemoryRegions() const override {
        return {{0x1000u, 0x1000u, 0x20000u, cerfcore::MemoryKind::kRam}};
    }
};
class TestGpuMemory final : public cerfcore::Imx51Gpu3dMemory {
public:
    using Imx51Gpu3dMemory::Imx51Gpu3dMemory;
    bool ShouldRegister() override { return true; }
};
int main() {
    cerfcore::CerfEmulator emu({}, 0, nullptr);
    emu.AddCandidate<cerfcore::Fatal>(std::make_unique<cerfcore::Fatal>(emu), false);
    emu.AddCandidate<cerfcore::JitRunner>(std::make_unique<cerfcore::JitRunner>(emu), false);
    emu.AddCandidate<cerfcore::PeripheralDispatcher>(std::make_unique<cerfcore::PeripheralDispatcher>(emu), false);
    emu.AddCandidate<cerfcore::PageTableBuilder>(std::make_unique<TestMemoryMap>(emu), false);
    emu.AddCandidate<cerfcore::EmulatedMemory>(std::make_unique<cerfcore::EmulatedMemory>(emu), false);
    emu.AddCandidate<cerfcore::Imx51Gpu3dMemory>(std::make_unique<TestGpuMemory>(emu), false);
    auto& memory = emu.Get<cerfcore::EmulatedMemory>();
    constexpr uint32_t source = 0x1000u, destination = 0x10000u;
    for (uint32_t word = 0; word < 128u * 96u / 2u; ++word) {
        memory.WriteWord(source + word * 4u, 0x12340000u ^ word);
        memory.WriteWord(destination + word * 4u, 0xdeadbeefu);
    }
    const auto bits = [](float value) { return std::bit_cast<uint32_t>(value); };
    std::unordered_map<uint32_t, uint32_t> regs = {
        {0x2205u, 0x240u}, {0x2001u, destination | 0x252u}, {0x2000u, 128u},
        {0x0a2au, source}, {0x4806u, 0x01004802u}, {0x4807u, source | 4u},
        {0x4808u, ((96u - 1u) << 13) | (128u - 1u)}, {0x4809u, 0x01001414u},
        {0x2202u, 0xc20u}, {0x2104u, 15u}, {0x2206u, 0xb00u},
        {0x4048u, bits(64.0f)}, {0x4049u, bits(32.0f)},
        {0x404au, bits(64.0f)}, {0x404bu, bits(32.0f)},
        {0x4098u, bits(0.5f)}, {0x4099u, bits(1.0f / 3.0f)},
        {0x409au, bits(0.5f)}, {0x409bu, bits(1.0f / 3.0f)},
        {0x2081u, 0x80000000u}, {0x2082u, (64u << 16) | 128u},
    };
    cerfcore::Imx51Gpu3dBlit blit(emu);
    blit.Draw(0x00040086u, 0, regs, 0);
    for (uint32_t word = 0; word < 128u * 96u / 2u; ++word) {
        const uint32_t expected = word < 128u * 64u / 2u ? 0x12340000u ^ word : 0xdeadbeefu;
        const uint32_t actual = memory.ReadWord(destination + word * 4u);
        if (actual != expected) {
            std::printf("word %u: got %08x expected %08x\n", word, actual, expected);
            return 1;
        }
    }
    return 0;
}

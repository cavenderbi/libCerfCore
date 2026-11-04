#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/boards/board_context.h>
#include "sharp_mobilon_hc_family_id.h"
#include "sharp_mobilon_hc_memory.h"

#include <cstddef>
#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kCs1Base = 0x10000000u;
constexpr uint32_t kCs1Size = 0x00400000u;

constexpr uint32_t kRamTagOffset = 0x0001FF00u;
constexpr uint8_t  kRamTag[] = {'1', '2', 'M', 'B'};

class SharpMobilonHcCs1 : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SharpMobilonHc;
    }

    void OnReady() override {
        ram_tag_ = emu_.Get<SharpMobilonHcMemory>().Cs1RamTag();
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return kCs1Base; }
    uint32_t MmioSize() const override { return kCs1Size; }

    uint8_t  ReadByte (uint32_t addr) override { return uint8_t(Read(addr, 1)); }
    uint16_t ReadHalf (uint32_t addr) override { return uint16_t(Read(addr, 2)); }
    uint32_t ReadWord (uint32_t addr) override { return uint32_t(Read(addr, 4)); }
    uint64_t ReadDword(uint32_t addr) override { return Read(addr, 8); }

private:
    uint64_t Read(uint32_t addr, size_t width) {
        uint8_t bytes[8];
        bool undriven = false;
        for (size_t i = 0; i < width; ++i) {
            const uint32_t tag_index = addr - kCs1Base + uint32_t(i) - kRamTagOffset;
            if (ram_tag_ && tag_index < sizeof(kRamTag)) {
                bytes[i] = kRamTag[tag_index];
            } else {
                bytes[i] = 0xFFu;
                undriven = true;
            }
        }
        if (undriven) NoteFloat(addr);
        return cerf::le::UN(bytes, width);
    }

    void NoteFloat(uint32_t addr) {
        if (floats_++ == 0u) {
            LOG(Mem, "SharpMobilonHcCs1: CS1 read at pa=0x%08X returns an undriven bus\n", addr);
        }
    }

    bool     ram_tag_ = false;
    uint32_t floats_  = 0;
};

}

REGISTER_SERVICE(SharpMobilonHcCs1);

}

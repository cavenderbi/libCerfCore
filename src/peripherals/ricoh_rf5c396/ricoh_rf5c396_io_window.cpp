#include "ricoh_rf5c396.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../peripheral_dispatcher.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kSize = 0x00010000u;

constexpr uint32_t kPcicIndexOffset = 0x3E0u;
constexpr uint32_t kPcicDataOffset  = 0x3E1u;

class RicohRf5c396IoWindow : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return RicohRf5c396::FittedOnFamily(emu_.Get<BoardContext>().GetBoardFamilyId());
    }

    void OnReady() override {
        base_ = emu_.Get<RicohRf5c396Board>().IsaIoBase();
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return base_; }
    uint32_t MmioSize() const override { return kSize; }

    void SaveState(StateWriter& w) override { emu_.Get<RicohRf5c396>().SaveState(w); }
    void RestoreState(StateReader& r) override { emu_.Get<RicohRf5c396>().RestoreState(r); }
    void PostRestore() override { emu_.Get<RicohRf5c396>().PostRestore(); }

    uint8_t ReadByte(uint32_t addr) override {
        const uint32_t off = addr - base_;
        auto& pcic = emu_.Get<RicohRf5c396>();
        if (off == kPcicIndexOffset || off == kPcicDataOffset)
            return pcic.ReadPcicByte(off - kPcicIndexOffset);
        return pcic.ReadCardIo8(off);
    }
    uint16_t ReadHalf(uint32_t addr) override {
        const uint32_t off = addr - base_;
        auto& pcic = emu_.Get<RicohRf5c396>();
        if (off == kPcicIndexOffset) {
            const uint16_t index = pcic.ReadPcicByte(0u);
            const uint16_t data = pcic.ReadPcicByte(1u);
            return static_cast<uint16_t>(index | (data << 8));
        }
        return pcic.ReadCardIo16(off);
    }
    void WriteByte(uint32_t addr, uint8_t value) override {
        const uint32_t off = addr - base_;
        auto& pcic = emu_.Get<RicohRf5c396>();
        if (off == kPcicIndexOffset || off == kPcicDataOffset) {
            pcic.WritePcicByte(off - kPcicIndexOffset, value);
            return;
        }
        pcic.WriteCardIo8(off, value);
    }
    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t off = addr - base_;
        auto& pcic = emu_.Get<RicohRf5c396>();
        if (off == kPcicIndexOffset) {
            pcic.WritePcicByte(0u, static_cast<uint8_t>(value & 0xFFu));
            pcic.WritePcicByte(1u, static_cast<uint8_t>(value >> 8));
            return;
        }
        pcic.WriteCardIo16(off, value);
    }

private:
    uint32_t base_ = 0u;
};

}

REGISTER_SERVICE(RicohRf5c396IoWindow);

}

#include "pd6710_controller.h"

#include <cerfcore/boards/board_context.h>
#include "../../boards/samsung_smdk2410/samsung_smdk2410_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../peripheral_dispatcher.h"
#include "../pcmcia/pcmcia_host_cycles.h"
#include "../pcmcia/pcmcia_slot.h"
#include <cerfcore/state/state_stream.h>

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x11000000u;
constexpr uint32_t kSize = 0x00010000u;

constexpr uint32_t kPcicIndexOffset = 0x3E0u;
constexpr uint32_t kPcicDataOffset  = 0x3E1u;

class Pd6710IoWindow : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SamsungSmdk2410;
    }
    void OnReady() override {
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    /* The single forward site for the Cirrus slot (a Service off the walk);
       the mem-window peripheral deliberately does not duplicate it. */
    void SaveState(StateWriter& w) override {
        emu_.Get<Pd6710Controller>().SaveState(w);
    }
    void RestoreState(StateReader& r) override {
        emu_.Get<Pd6710Controller>().RestoreState(r);
    }
    void PostRestore() override {
        emu_.Get<Pd6710Controller>().PostRestore();
    }

    uint8_t  ReadByte (uint32_t addr) override;
    uint16_t ReadHalf (uint32_t addr) override;
    void     WriteByte (uint32_t addr, uint8_t  value) override;
    void     WriteHalf (uint32_t addr, uint16_t value) override;

private:
    void RequireOneCardCycle(Pd6710Controller& ctl, uint32_t off, uint32_t card_io, PcmciaIoWidth width,
                             const char* access);

    FloatingBus floating_;
};

}  /* namespace */

void Pd6710IoWindow::RequireOneCardCycle(Pd6710Controller& ctl, uint32_t off, uint32_t card_io,
                                         PcmciaIoWidth width, const char* access) {
    if (IoCardCycles(ctl.Slot(), card_io, width, 2u) > 1u)
        emu_.Get<Fatal>().Die("[PD6710] 16-bit I/O %s at +0x%X through an 8-bit window; IOCS16* is not "
                              "connected on this board, so no host splits it", access, off);
}

uint8_t Pd6710IoWindow::ReadByte(uint32_t addr) {
    const uint32_t off = addr - kBase;
    auto& ctl = emu_.Get<Pd6710Controller>();
    if (off == kPcicIndexOffset || off == kPcicDataOffset) {
        return ctl.ReadPcicByte(off - kPcicIndexOffset);
    }
    uint32_t card_io = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    if (!ctl.MapIo(off, &card_io, &width))
        return floating_.Read8("PD6710 I/O", off);
    const uint32_t v = ReadCardIo(ctl.Slot(), card_io, width, 1u);
    if (ctl.InpackGated() && !ctl.Slot().InputAcknowledge(card_io))
        return floating_.Read8("PD6710 I/O read without -INPACK", off);
    return static_cast<uint8_t>(ctl.MaskFloppyStatusBit7(off, v, 1u));
}

uint16_t Pd6710IoWindow::ReadHalf(uint32_t addr) {
    const uint32_t off = addr - kBase;
    auto& ctl = emu_.Get<Pd6710Controller>();
    if (off == kPcicIndexOffset) {
        const uint16_t index = ctl.ReadPcicByte(0u);
        const uint16_t data  = ctl.ReadPcicByte(kPcicDataOffset - kPcicIndexOffset);
        return static_cast<uint16_t>(index | (data << 8));
    }
    uint32_t card_io = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    if (!ctl.MapIo(off, &card_io, &width))
        return floating_.Read16("PD6710 I/O", off);
    RequireOneCardCycle(ctl, off, card_io, width, "read");
    const uint32_t v = ReadCardIo(ctl.Slot(), card_io, width, 2u);
    if (ctl.InpackGated() && !ctl.Slot().InputAcknowledge(card_io))
        return floating_.Read16("PD6710 I/O read without -INPACK", off);
    return static_cast<uint16_t>(ctl.MaskFloppyStatusBit7(off, v, 2u));
}

void Pd6710IoWindow::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off = addr - kBase;
    auto& ctl = emu_.Get<Pd6710Controller>();
    if (off == kPcicIndexOffset || off == kPcicDataOffset) {
        ctl.WritePcicByte(off - kPcicIndexOffset, value);
        return;
    }
    uint32_t card_io = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    if (!ctl.MapIo(off, &card_io, &width)) {
        floating_.Write("PD6710 I/O", 8u, off);
        return;
    }
    WriteCardIo(ctl.Slot(), card_io, width, 1u, value);
}

void Pd6710IoWindow::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t off = addr - kBase;
    auto& ctl = emu_.Get<Pd6710Controller>();
    if (off == kPcicIndexOffset)
        emu_.Get<Fatal>().Die("[PD6710] 16-bit write 0x%04X at the register port; the CL-PD6710 data sheet "
                              "documents 16-bit reads there only", value);
    uint32_t card_io = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    if (!ctl.MapIo(off, &card_io, &width)) {
        floating_.Write("PD6710 I/O", 16u, off);
        return;
    }
    RequireOneCardCycle(ctl, off, card_io, width, "write");
    WriteCardIo(ctl.Slot(), card_io, width, 2u, value);
}

REGISTER_SERVICE(Pd6710IoWindow);

}

#include "pd6710_controller.h"

#include <cerfcore/boards/board_context.h>
#include "../../boards/samsung_smdk2410/samsung_smdk2410_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../peripheral_dispatcher.h"
#include "../pcmcia/pcmcia_host_cycles.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x10000000u;
constexpr uint32_t kSize = 0x01000000u;

class Pd6710MemWindow : public Peripheral {
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

    uint8_t  ReadByte (uint32_t addr) override;
    uint16_t ReadHalf (uint32_t addr) override;
    void     WriteByte (uint32_t addr, uint8_t  value) override;
    void     WriteHalf (uint32_t addr, uint16_t value) override;

private:
    void RequireSixteenBitWindow(bool sixteen, uint32_t off, const char* access);

    FloatingBus floating_;
};

}  /* namespace */

uint8_t Pd6710MemWindow::ReadByte(uint32_t addr) {
    const uint32_t off = addr - kBase;
    auto& ctl = emu_.Get<Pd6710Controller>();
    uint32_t card_addr;
    bool attribute, writable, sixteen;
    if (!ctl.MapMem(off, &card_addr, &attribute, &writable, &sixteen))
        return floating_.Read8("PD6710 memory", off);
    return static_cast<uint8_t>(ReadCardMemory(ctl.Slot(), card_addr, attribute, sixteen, 1u));
}

uint16_t Pd6710MemWindow::ReadHalf(uint32_t addr) {
    const uint32_t off = addr - kBase;
    auto& ctl = emu_.Get<Pd6710Controller>();
    uint32_t card_addr;
    bool attribute, writable, sixteen;
    if (!ctl.MapMem(off, &card_addr, &attribute, &writable, &sixteen))
        return floating_.Read16("PD6710 memory", off);
    RequireSixteenBitWindow(sixteen, off, "read");
    return static_cast<uint16_t>(ReadCardMemory(ctl.Slot(), card_addr, attribute, sixteen, 2u));
}

void Pd6710MemWindow::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off = addr - kBase;
    auto& ctl = emu_.Get<Pd6710Controller>();
    uint32_t card_addr;
    bool attribute, writable, sixteen;
    if (!ctl.MapMem(off, &card_addr, &attribute, &writable, &sixteen)) {
        floating_.Write("PD6710 memory", 8u, off);
        return;
    }
    if (!writable) {
        LOG(Pcmcia, "[MemWin] write8 +0x%X = 0x%02X (window write-"
                "protected)\n", off, value);
        return;
    }
    WriteCardMemory(ctl.Slot(), card_addr, attribute, sixteen, 1u, value);
}

void Pd6710MemWindow::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t off = addr - kBase;
    auto& ctl = emu_.Get<Pd6710Controller>();
    uint32_t card_addr;
    bool attribute, writable, sixteen;
    if (!ctl.MapMem(off, &card_addr, &attribute, &writable, &sixteen)) {
        floating_.Write("PD6710 memory", 16u, off);
        return;
    }
    if (!writable) {
        LOG(Pcmcia, "[MemWin] write16 +0x%X = 0x%04X (window write-"
                "protected)\n", off, value);
        return;
    }
    RequireSixteenBitWindow(sixteen, off, "write");
    WriteCardMemory(ctl.Slot(), card_addr, attribute, sixteen, 2u, value);
}

void Pd6710MemWindow::RequireSixteenBitWindow(bool sixteen, uint32_t off, const char* access) {
    if (!sixteen)
        emu_.Get<Fatal>().Die("[PD6710] 16-bit memory %s at +0x%X through an 8-bit window; MEMCS16* is not "
                              "connected on this board, so no host splits it", access, off);
}

REGISTER_SERVICE(Pd6710MemWindow);

}

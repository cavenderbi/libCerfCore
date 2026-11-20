#include "pci1225_card_cycles.h"

#include "../pcic82365/pcic82365.h"
#include "../pcmcia/pcmcia_host_cycles.h"
#include "pci1225_config_space.h"
#include "pci1225_socket_registers.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>

namespace cerfcore {

namespace {

constexpr uint32_t kZoomVideoCardLines = 0xFu << 22;

}

Pci1225CardCycles::Pci1225CardCycles(CerfEmulator& emu, std::mutex& mtx, Pci1225ConfigSpace& config, Pcic82365& exca,
                                     Pci1225SocketRegisters& socket_regs, PcmciaSlot& slot)
    : emu_(emu), mtx_(mtx), config_(config), exca_(exca), socket_regs_(socket_regs), slot_(slot) {}

bool Pci1225CardCycles::MemHit(uint32_t addr) const {
    std::lock_guard<std::mutex> lk(mtx_);
    uint32_t card = 0u;
    bool attr = false;
    bool wr = false;
    bool sixteen = false;
    return config_.MemorySpaceEnabled() && exca_.MapMem(addr, &card, &attr, &wr, &sixteen);
}

uint32_t Pci1225CardCycles::MemRead(uint32_t addr, unsigned size) {
    uint32_t card = 0u;
    bool attr = false;
    bool wr = false;
    bool sixteen = false;
    MapMem(addr, &card, &attr, &wr, &sixteen);
    const unsigned cycles = MemoryCardCycles(sixteen, size);
    if (cycles > 1u)
        RefuseCycleOrder("memory read", addr, size, cycles);
    return ReadCardMemory(slot_, card, attr, sixteen, size);
}

void Pci1225CardCycles::MemWrite(uint32_t addr, uint32_t value, unsigned size) {
    uint32_t card = 0u;
    bool attr = false;
    bool wr = false;
    bool sixteen = false;
    MapMem(addr, &card, &attr, &wr, &sixteen);
    if (!wr)
        return;
    const unsigned cycles = MemoryCardCycles(sixteen, size);
    if (cycles > 1u)
        RefuseCycleOrder("memory write", addr, size, cycles);
    WriteCardMemory(slot_, card, attr, sixteen, size, value);
}

bool Pci1225CardCycles::IoMappedLocked(uint32_t pci_io) const {
    uint32_t card = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    return config_.IoSpaceEnabled() && exca_.MapIo(pci_io, &card, &width);
}

uint32_t Pci1225CardCycles::IoRead(uint32_t pci_io, unsigned size) {
    uint32_t card = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    MapIo(pci_io, &card, &width);
    RequireIoCycleOrder("I/O read", pci_io, card, width, size);
    return ReadCardIo(slot_, card, width, size);
}

void Pci1225CardCycles::IoWrite(uint32_t pci_io, uint32_t value, unsigned size) {
    uint32_t card = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    MapIo(pci_io, &card, &width);
    RequireIoCycleOrder("I/O write", pci_io, card, width, size);
    WriteCardIo(slot_, card, width, size, value);
}

void Pci1225CardCycles::MapMem(uint32_t addr, uint32_t* card, bool* attr, bool* wr, bool* sixteen) {
    std::lock_guard<std::mutex> lk(mtx_);
    exca_.MapMem(addr, card, attr, wr, sixteen);
    if (config_.ReducedZoomVideo())
        *card &= ~kZoomVideoCardLines;
    config_.NoteSocketActivity();
    socket_regs_.NoteSocketAccess();
}

void Pci1225CardCycles::MapIo(uint32_t pci_io, uint32_t* card, PcmciaIoWidth* width) {
    std::lock_guard<std::mutex> lk(mtx_);
    exca_.MapIo(pci_io, card, width);
    config_.NoteSocketActivity();
    socket_regs_.NoteSocketAccess();
}

void Pci1225CardCycles::RequireIoCycleOrder(const char* access, uint32_t pci_io, uint32_t card, PcmciaIoWidth width,
                                            unsigned size) {
    const unsigned cycles = IoCardCycles(slot_, card, width, size);
    if (cycles > 1u && !(size == 2u && width == PcmciaIoWidth::kCardIois16))
        RefuseCycleOrder(access, pci_io, size, cycles);
}

void Pci1225CardCycles::RefuseCycleOrder(const char* access, uint32_t addr, unsigned size, unsigned cycles) {
    emu_.Get<Fatal>().Die("PCI1225 function %u: %u-byte %s at 0x%08X needs %u card cycles; SCPS035B gives "
                          "the cycle order only for an IOIS16#-sized I/O byte pair",
                          config_.Function(), size, access, addr, cycles);
}

}

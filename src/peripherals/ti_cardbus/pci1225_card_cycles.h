#pragma once

#include "../pcmcia/pcmcia_io_width.h"
#include "../pcmcia/pcmcia_slot.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

class CerfEmulator;
class Pci1225ConfigSpace;
class Pci1225SocketRegisters;
class Pcic82365;

class Pci1225CardCycles {
public:
    Pci1225CardCycles(CerfEmulator& emu, std::mutex& mtx, Pci1225ConfigSpace& config, Pcic82365& exca,
                      Pci1225SocketRegisters& socket_regs, PcmciaSlot& slot);

    bool     MemHit(uint32_t addr) const;
    uint32_t MemRead(uint32_t addr, unsigned size);
    void     MemWrite(uint32_t addr, uint32_t value, unsigned size);

    bool     IoMappedLocked(uint32_t pci_io) const;
    uint32_t IoRead(uint32_t pci_io, unsigned size);
    void     IoWrite(uint32_t pci_io, uint32_t value, unsigned size);

private:
    void MapMem(uint32_t addr, uint32_t* card, bool* attr, bool* wr, bool* sixteen);
    void MapIo(uint32_t pci_io, uint32_t* card, PcmciaIoWidth* width);
    void RequireIoCycleOrder(const char* access, uint32_t pci_io, uint32_t card, PcmciaIoWidth width,
                             unsigned size);
    [[noreturn]] void RefuseCycleOrder(const char* access, uint32_t addr, unsigned size, unsigned cycles);

    CerfEmulator&           emu_;
    std::mutex&             mtx_;
    Pci1225ConfigSpace&     config_;
    Pcic82365&              exca_;
    Pci1225SocketRegisters& socket_regs_;
    PcmciaSlot&             slot_;
};

}

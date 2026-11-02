#pragma once

#include "../pci/pci_device.h"
#include "../pcic82365/exca_socket_sequencer.h"
#include "../pcic82365/pcic82365.h"
#include "../pcmcia/pcmcia_slot.h"
#include "pci1225_config_space.h"
#include "pci1225_exca_chip_registers.h"
#include "pci1225_interrupt_pin.h"
#include "pci1225_socket_registers.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

class CerfEmulator;
class Pci1225LegacyPort;
class TiCardbusBoard;

class Pci1225SocketFunction : public PciDevice, public PcmciaSlotHost {
public:
    Pci1225SocketFunction(CerfEmulator& emu, uint8_t function, const char* slot_label,
                          Pci1225SharedConfig& shared, std::mutex& mtx, ExcaSocketSequencer& sequencer);

    void Attach(TiCardbusBoard& board, Pci1225SocketFunction& peer, Pci1225LegacyPort* legacy);

    PcmciaSlot& Slot() { return slot_; }
    void ResetLocked();
    void UpdateLineLocked(bool force);

    uint32_t ExCaRead(uint32_t idx, unsigned size);
    void     ExCaWrite(uint32_t idx, uint32_t value, unsigned size);

    uint8_t PciDev() const override;
    uint8_t PciFnc() const override { return config_.Function(); }

    uint32_t ConfigRead(uint32_t reg) override;
    void     ConfigWrite(uint32_t reg, uint32_t value) override;

    bool     MemClaims(uint32_t addr) const override;
    uint32_t MemRead(uint32_t addr, unsigned size) override;
    void     MemWrite(uint32_t addr, uint32_t value, unsigned size) override;

    bool     IoClaims(uint32_t pci_io) const override;
    uint32_t IoRead(uint32_t pci_io, unsigned size) override;
    void     IoWrite(uint32_t pci_io, uint32_t value, unsigned size) override;

    void OnCardDetectChanged(PcmciaSlot& slot) override;
    void OnReadyIreqLow(PcmciaSlot& slot) override;
    void OnReadyIreqHigh(PcmciaSlot& slot) override;
    uint8_t UndrivenDataLane() const override { return kUnstatedUndrivenDataLane; }

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

private:
    bool CardStsLevelLocked() const;
    Pci1225CardPins CardPins() const;
    bool SocketBarHit(uint32_t addr) const;
    bool CardMemHit(uint32_t addr) const;
    void MapClaimedMem(uint32_t addr, uint32_t* card, bool* attr, bool* wr, bool* sixteen);
    void MapClaimedIo(uint32_t pci_io, uint32_t* card, PcmciaIoWidth* width);
    uint32_t CardMemRead(uint32_t addr, unsigned size);
    void     CardMemWrite(uint32_t addr, uint32_t value, unsigned size);
    bool     LegacyClaimLocked(uint32_t pci_io) const;
    uint32_t SocketRead(uint32_t off, unsigned size);
    void     SocketWrite(uint32_t off, uint32_t value, unsigned size);
    void     WriteSocketControl(uint32_t value);
    void     SetReadyIreqLow(bool low);
    void     ApplyGlobalControlLocked();
    uint8_t  ExCaReadByte(uint32_t idx);
    void     ExCaWriteByte(uint32_t idx, uint8_t value);
    void     NoteExcaPowerRequestLocked(uint8_t before, uint8_t now);
    bool     CscRoutedToPciLocked() const;
    void RequireIoCycleOrder(const char* access, uint32_t pci_io, uint32_t card, PcmciaIoWidth width,
                             unsigned size);
    [[noreturn]] void RefuseCycleOrder(const char* access, uint32_t addr, unsigned size, unsigned cycles);
    [[noreturn]] void RefuseExCa(const char* access, uint32_t idx);

    CerfEmulator&          emu_;
    Pci1225SharedConfig&   shared_;
    std::mutex&            mtx_;
    ExcaSocketSequencer&   sequencer_;
    TiCardbusBoard*        board_  = nullptr;
    Pci1225SocketFunction* peer_   = nullptr;
    Pci1225LegacyPort*     legacy_ = nullptr;

    PcmciaSlot             slot_;
    Pcic82365              exca_;
    Pci1225ConfigSpace     config_;
    Pci1225SocketRegisters socket_regs_;
    Pci1225ExcaChipRegisters chip_regs_;
    Pci1225InterruptPin    pin_;
    bool                   functional_level_ = false;
    bool                   cardsts_level_    = false;
};

}

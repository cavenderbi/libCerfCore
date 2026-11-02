#pragma once

#include <cerfcore/core/service.h>
#include "../floating_bus.h"
#include "../pcic82365/exca_socket_sequencer.h"
#include "../pcic82365/pcic82365.h"
#include "../pcmcia/pcmcia_slot.h"

#include <array>
#include <cstdint>
#include <mutex>

namespace cerfcore {

/* Cirrus PD6710 single-socket PCMCIA host adapter. The PCIC index/data
   port pair and the window mapping registers live here; the I/O and
   memory window peripherals decode guest accesses through MapIo /
   MapMem and reach the card via Slot(). */
class Pd6710Controller : public Service, public PcmciaSlotHost {
public:
    explicit Pd6710Controller(CerfEmulator& emu);

    bool ShouldRegister() override;
    void OnReady() override;
    void OnShutdown() override { slot_.OnShutdown(); }

    PcmciaSlot& Slot() { return slot_; }

    /* PCIC register file + the resident card (forwarded from the I/O window
       peripheral, which is on the hibernation walk; the controller is not). */
    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);
    void PostRestore();

    uint8_t ReadPcicByte (uint32_t port);
    void    WritePcicByte(uint32_t port, uint8_t value);

    bool     MapIo(uint32_t bus_io, uint32_t* card_io, PcmciaIoWidth* width);
    bool     MapMem(uint32_t bus_off, uint32_t* card_addr,
                    bool* attribute, bool* writable, bool* sixteen_bit);
    uint32_t MaskFloppyStatusBit7(uint32_t bus_io, uint32_t value, unsigned bytes);
    bool     InpackGated();

    void OnCardDetectChanged(PcmciaSlot& slot) override;
    void OnReadyIreqLow     (PcmciaSlot& slot) override;
    void OnReadyIreqHigh    (PcmciaSlot& slot) override;
    uint8_t UndrivenDataLane() const override { return kUnstatedUndrivenDataLane; }

private:
    uint8_t ReadIndexedDataLocked();
    void WriteIndexedDataLocked(uint8_t value, ExcaWriteEffect* effect);
    void SetReadyIreqLow(bool low);
    void ResetController();
    void ResetRegistersLocked();
    bool ManagementToIntrLocked() const;
    bool PulseManagementLocked() const;
    bool RoutedToWiredIrqLocked(const char* source, uint8_t select) const;
    void RouteManagementLocked(uint8_t* to_intr, uint8_t* to_irq_pin) const;
    void UpdateManagementLineLocked();
    void DriveCardLineLocked(bool force);
    bool SuspendedLocked() const;
    [[noreturn]] void RefuseIndexLocked(const char* access);

    PcmciaSlot slot_;
    ExcaSocketSequencer sequencer_;

    mutable std::mutex state_mutex_;

    uint8_t index_ = 0u;
    uint8_t intr_bits_ = 0u;
    uint8_t irq_pin_bits_ = 0u;
    bool    card_line_level_ = false;
    uint8_t misc_ctrl1_ = 0u;
    uint8_t fifo_scratch_ = 0u;
    uint8_t misc_ctrl2_ = 0u;
    uint8_t ata_control_ = 0u;
    uint8_t scratchpad_ = 0u;
    bool    card_request_ = false;
    std::array<uint8_t, 6> timing_ = {};
    FloatingBus floating_;

    uint8_t reg_chip_info_ = 0u;   /* PD6710-specific chip-detect probe register (0x1F) */

    Pcic82365 exca_;
};

}

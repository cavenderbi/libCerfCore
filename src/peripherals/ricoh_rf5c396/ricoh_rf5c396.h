#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "../floating_bus.h"
#include "../pcic82365/exca_socket_sequencer.h"
#include "../pcic82365/pcic82365.h"
#include "../pcmcia/pcmcia_slot.h"
#include "ricoh_rf5c396_board.h"
#include "ricoh_rf5c396_chip_registers.h"

#include <cstdint>
#include <mutex>
#include <string_view>

namespace cerfcore {

class RicohRf5c396 : public Service, public PcmciaSlotHost {
public:
    explicit RicohRf5c396(CerfEmulator& emu);

    static bool FittedOnFamily(std::string_view family);

    bool ShouldRegister() override;
    void OnReady() override;
    void OnShutdown() override {
        slot0_.OnShutdown();
        slot1_.OnShutdown();
    }

    uint8_t ReadPcicByte (uint32_t port);
    void    WritePcicByte(uint32_t port, uint8_t value);

    uint8_t  ReadCardMem8 (uint32_t off);
    uint16_t ReadCardMem16(uint32_t off);
    void     WriteCardMem8 (uint32_t off, uint8_t  value);
    void     WriteCardMem16(uint32_t off, uint16_t value);

    uint8_t  ReadCardIo8 (uint32_t off);
    uint16_t ReadCardIo16(uint32_t off);
    void     WriteCardIo8 (uint32_t off, uint8_t  value);
    void     WriteCardIo16(uint32_t off, uint16_t value);

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);
    void PostRestore();

    void OnCardDetectChanged(PcmciaSlot& slot) override;
    void OnReadyIreqLow     (PcmciaSlot& slot) override;
    void OnReadyIreqHigh    (PcmciaSlot& slot) override;
    uint8_t UndrivenDataLane() const override { return 0u; }

private:
    int         BankOf(const PcmciaSlot& slot) const { return &slot == &slot1_ ? 1 : 0; }
    PcmciaSlot& Slot(int bank) { return bank ? slot1_ : slot0_; }
    Pcic82365&  Exca(int bank) { return bank ? exca1_ : exca0_; }

    uint8_t ReadDataLocked(uint32_t port);
    void    WriteDataLocked(int bank, uint8_t value, ExcaWriteEffect* effect);
    void    WriteChipRegisterLocked(int bank, uint8_t sub, uint8_t value, ExcaWriteEffect* effect);
    void    ApplyChipModesLocked();
    bool    GpiLowLocked(int bank);
    void    TrackGpiLocked(int bank);
    bool    CardInterruptLocked(int bank);
    void    UpdateIrqLineLocked();
    void    ResetController();

    struct CardMemHit {
        int      bank        = 0;
        uint32_t addr        = 0u;
        bool     attribute   = false;
        bool     writable    = false;
        bool     sixteen_bit = false;
        bool     isolated    = false;
    };
    struct CardIoHit {
        int           bank         = 0;
        uint32_t      io           = 0u;
        PcmciaIoWidth width        = PcmciaIoWidth::kEightBit;
        bool          isolated     = false;
        bool          inpack_gated = false;
    };

    bool     DecodeMem(uint32_t off, CardMemHit* hit);
    bool     DecodeIo (uint32_t off, CardIoHit* hit);
    bool     Memcs16Asserted(uint32_t off);
    uint32_t MaskFloppyStatusBit7(int bank, uint32_t off, uint32_t value, unsigned bytes);

    PcmciaSlot slot0_;
    PcmciaSlot slot1_;
    Pcic82365  exca0_;
    Pcic82365  exca1_;
    ExcaSocketSequencer sequencer_;

    RicohRf5c396Board* board_ = nullptr;

    mutable std::mutex state_mutex_;
    uint8_t index_ = 0u;
    RicohRf5c396ChipRegisters chip_;
    bool gpi_low_[2] = { false, false };
    FloatingBus floating_;
};

}

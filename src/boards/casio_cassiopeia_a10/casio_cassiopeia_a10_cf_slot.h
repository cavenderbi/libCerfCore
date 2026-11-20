#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/casio_pc_card_socket/casio_pc_card_status.h"
#include "../../peripherals/pcmcia/pcmcia_slot.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

class CasioCassiopeiaA10ChipIntc;
class CasioCassiopeiaA10PinRegisters;

class CasioCassiopeiaA10CfSlot : public Peripheral, public PcmciaSlotHost {
public:
    explicit CasioCassiopeiaA10CfSlot(CerfEmulator& emu);

    bool ShouldRegister() override;
    void OnReady() override;
    void OnShutdown() override;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

    void    OnCardDetectChanged(PcmciaSlot& slot) override;
    void    OnReadyIreqLow(PcmciaSlot& slot) override;
    void    OnReadyIreqHigh(PcmciaSlot& slot) override;
    void    OnStatusChange(PcmciaSlot&, bool) override {}
    uint8_t UndrivenDataLane() const override { return kUnstatedUndrivenDataLane; }

    PcmciaSlot& Slot();
    bool        AttributeSelected();

private:
    uint16_t Status() const;
    void     ApplyControl(uint16_t control);
    void     DriveIreqLine();
    void     DriveCardChangeLocked();
    void     Reset();

    CasioCassiopeiaA10ChipIntc*     chip_intc_ = nullptr;
    CasioCassiopeiaA10PinRegisters* pins_      = nullptr;
    PcmciaSlot                      slot_;
    CasioPcCardStatus               status_;
    std::mutex                      mtx_;
    uint16_t                        control_     = 0u;
    bool                            card_change_ = false;
};

}

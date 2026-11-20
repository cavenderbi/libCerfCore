#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include <cerfcore/core/callback_list.h>
#include "../../peripherals/casio_pc_card_socket/casio_pc_card_status.h"
#include "../../peripherals/pcmcia/pcmcia_slot.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

class CasioCassiopeiaA10CardWiring;

class CasioCassiopeiaA10PcCard : public Peripheral, public PcmciaSlotHost {
public:
    explicit CasioCassiopeiaA10PcCard(CerfEmulator& emu);

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
    void    OnStatusChange(PcmciaSlot& slot, bool low) override;
    uint8_t UndrivenDataLane() const override { return kUnstatedUndrivenDataLane; }

    PcmciaSlot& Slot();
    bool        SocketVccOn();
    void        ApplyPower();
    void        AddCardDetectListener(CallbackList<>::Callback listener);

private:
    void WriteEvents(uint32_t addr, uint16_t value);
    void DriveLinesLocked();
    void Reset();

    CasioCassiopeiaA10CardWiring* wiring_ = nullptr;
    CallbackList<>                detect_listeners_;
    PcmciaSlot                    slot_;
    CasioPcCardStatus             status_;
    std::mutex                    mtx_;
    uint16_t                      enables_  = 0u;
    uint16_t                      upper_    = 0u;
    uint16_t                      latches_  = 0u;
    uint16_t                      detect_   = 0u;
    uint16_t                      power_    = 0u;
    uint16_t                      reset_    = 0u;
    uint16_t                      vcc_      = 0u;
    uint16_t                      speaker_route_ = 0u;
};

}

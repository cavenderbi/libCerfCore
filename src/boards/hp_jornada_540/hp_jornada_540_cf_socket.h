#pragma once

#include <cerfcore/core/service.h>
#include "../../peripherals/pcmcia/pcmcia_slot.h"

#include <atomic>

namespace cerfcore {

class Sh77xxIntc;
class StateReader;
class StateWriter;

class HpJornada540CfSocket : public Service, public PcmciaSlotHost {
public:
    explicit HpJornada540CfSocket(CerfEmulator& emu);

    bool ShouldRegister() override;
    void OnReady() override;
    void OnShutdown() override;

    PcmciaSlot& Slot() { return slot_; }
    bool        CardAbsent() const { return !slot_.HasCard(); }
    void        SetCardPower(bool on);
    void        SetResetLine(bool asserted);

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

    void    OnCardDetectChanged(PcmciaSlot& slot) override;
    void    OnReadyIreqLow(PcmciaSlot& slot) override;
    void    OnReadyIreqHigh(PcmciaSlot& slot) override;
    void    OnStatusChange(PcmciaSlot& slot, bool low) override;
    uint8_t UndrivenDataLane() const override { return kUnstatedUndrivenDataLane; }

private:
    void DriveLines();

    Sh77xxIntc*       intc_ = nullptr;
    PcmciaSlot        slot_;
    std::atomic<bool> ready_ireq_low_{false};
};

}

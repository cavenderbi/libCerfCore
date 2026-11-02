#pragma once

#include "ite_it8368.h"

#include "../pcmcia/pcmcia_slot.h"
#include <cerfcore/core/service.h>

namespace cerfcore {

class IteIt8368SocketHost : public Service,
                            public PcmciaSlotHost,
                            public IteIt8368IntSink {
public:
    IteIt8368SocketHost(CerfEmulator& emu, const char* slot_name);

    void OnReady() override;
    void OnShutdown() override;

    void OnCardDetectChanged(PcmciaSlot&) override;
    void OnReadyIreqLow(PcmciaSlot&) override;
    void OnReadyIreqHigh(PcmciaSlot&) override;
    uint8_t UndrivenDataLane() const override { return kUnstatedUndrivenDataLane; }

protected:
    PcmciaSlot& Slot() { return slot0_; }

private:
    PcmciaSlot slot0_;
};

}

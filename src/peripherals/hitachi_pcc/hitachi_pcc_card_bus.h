#pragma once

#include "../../socs/sh77xx/sh77xx_pcmcia_card_bus.h"
#include "hitachi_pcc.h"

#include <cstdint>

namespace cerfcore {

class HitachiPccCardBus : public Sh77xxPcmciaCardBus {
public:
    using Sh77xxPcmciaCardBus::Sh77xxPcmciaCardBus;

    uint32_t ReadCycles(uint32_t area, uint32_t offset, bool io, PcmciaIoWidth width, unsigned bytes) override;
    void     WriteCycles(uint32_t area, uint32_t offset, bool io, PcmciaIoWidth width, unsigned bytes,
                         uint32_t value) override;

protected:
    virtual HitachiPcc& Pcc() = 0;
};

}

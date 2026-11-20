#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include "vr4101_dma_channel.h"

#include <cstdint>

namespace cerfcore {

class Vr4101DmaAddresses : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual uint32_t Address(Vr4101DmaChannel channel) const             = 0;
    virtual bool     Step(Vr4101DmaChannel channel)                      = 0;
    virtual uint32_t HalfwordsToBoundary(Vr4101DmaChannel channel) const = 0;

    uint16_t Load(Vr4101DmaChannel channel);
    bool     Store(Vr4101DmaChannel channel, uint16_t word);
};

}

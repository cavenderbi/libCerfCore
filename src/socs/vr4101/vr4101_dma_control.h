#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include "vr4101_dma_channel.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace cerfcore {

class Vr4101DmaControl : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual bool TransferEnabled(Vr4101DmaChannel channel) const = 0;
    virtual void SetRequest(Vr4101DmaChannel channel, bool requested) = 0;

    void RegisterEnableListener(Vr4101DmaChannel channel, std::function<void()> fn);

protected:
    static uint16_t ChannelBit(Vr4101DmaChannel channel) {
        return static_cast<uint16_t>(1u << static_cast<uint32_t>(channel));
    }

    void TransfersEnabled(uint16_t channel_bits) const;

private:
    struct EnableListener {
        uint16_t              channel_bit;
        std::function<void()> fn;
    };
    std::vector<EnableListener> enable_listeners_;
};

}

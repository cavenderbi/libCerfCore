#include "vr4101_dma_control.h"

#include <utility>

namespace cerfcore {

void Vr4101DmaControl::RegisterEnableListener(Vr4101DmaChannel channel, std::function<void()> fn) {
    enable_listeners_.push_back(EnableListener{ChannelBit(channel), std::move(fn)});
}

void Vr4101DmaControl::TransfersEnabled(uint16_t channel_bits) const {
    for (const EnableListener& listener : enable_listeners_) {
        if ((channel_bits & listener.channel_bit) != 0u) listener.fn();
    }
}

}

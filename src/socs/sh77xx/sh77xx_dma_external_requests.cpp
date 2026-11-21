#include "sh77xx_dma_external_requests.h"

#include "sh77xx_pfc.h"
#include "sh77xx_port_wiring.h"

namespace cerfcore {

namespace {

struct DreqPin {
    uint32_t port;
    uint8_t  mask;
};

constexpr DreqPin kDreqPins[Sh77xxDmaExternalRequests::kChannels] = {{kSh77xxPortD, 0x10u}, {kSh77xxPortD, 0x40u}};

constexpr const char* kEdgeNames[Sh77xxDmaExternalRequests::kChannels]  = {"ch0_dreq_edge", "ch1_dreq_edge"};
constexpr const char* kBurstNames[Sh77xxDmaExternalRequests::kChannels] = {"ch0_dreq_burst", "ch1_dreq_burst"};

}

bool Sh77xxDmaExternalRequests::Wired(uint32_t channel) const { return wiring_->WiredDmaRequest(channel); }

bool Sh77xxDmaExternalRequests::Low(uint32_t channel) const {
    const DreqPin& pin = kDreqPins[channel];
    return Wired(channel) && (wiring_->NetLevels(pin.port) & pin.mask) == 0u;
}

bool Sh77xxDmaExternalRequests::Requested(uint32_t channel, bool edge_mode) const {
    if (!edge_mode) return Low(channel);
    return edge_[channel] || burst_[channel];
}

bool Sh77xxDmaExternalRequests::Sample(uint32_t channel, bool accepting) {
    const bool low = Low(channel);
    if (low && !seen_low_[channel] && accepting) edge_[channel] = true;
    seen_low_[channel] = low;
    return low;
}

void Sh77xxDmaExternalRequests::Consume(uint32_t channel, bool edge_mode, bool burst, bool done) {
    if (!edge_mode) return;
    edge_[channel]  = false;
    burst_[channel] = burst && !done;
}

void Sh77xxDmaExternalRequests::Drop(uint32_t channel) {
    edge_[channel]  = false;
    burst_[channel] = false;
}

void Sh77xxDmaExternalRequests::Resample() {
    for (uint32_t i = 0u; i < kChannels; ++i) seen_low_[i] = Low(i);
}

void Sh77xxDmaExternalRequests::Save(StateWriter& w) const {
    for (uint32_t i = 0u; i < kChannels; ++i) {
        w.Write(kEdgeNames[i], edge_[i]);
        w.Write(kBurstNames[i], burst_[i]);
    }
}

void Sh77xxDmaExternalRequests::Restore(StateReader& r) {
    for (uint32_t i = 0u; i < kChannels; ++i) {
        r.Read(kEdgeNames[i], edge_[i]);
        r.Read(kBurstNames[i], burst_[i]);
    }
}

}

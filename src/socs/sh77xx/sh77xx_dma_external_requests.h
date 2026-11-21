#pragma once

#include <cerfcore/state/state_stream.h>

#include <cstdint>

namespace cerfcore {

class Sh77xxPortWiring;

class Sh77xxDmaExternalRequests {
public:
    static constexpr uint32_t kChannels = 2u;

    void Attach(const Sh77xxPortWiring* wiring) { wiring_ = wiring; }

    bool Wired(uint32_t channel) const;
    bool Requested(uint32_t channel, bool edge_mode) const;
    bool Sample(uint32_t channel, bool accepting);
    void Consume(uint32_t channel, bool edge_mode, bool burst, bool done);
    void Drop(uint32_t channel);
    void Resample();

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);

private:
    bool Low(uint32_t channel) const;

    const Sh77xxPortWiring* wiring_ = nullptr;
    bool                    seen_low_[kChannels] = {};
    bool                    edge_[kChannels]     = {};
    bool                    burst_[kChannels]    = {};
};

}

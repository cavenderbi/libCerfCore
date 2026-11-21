#pragma once

#include "../execution/guest_cycle_clock.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

class AudioActivity;
class PacedWaveOut;

inline constexpr GuestCycleClock::Rate kHostPcmFlushRate{100u, 1u};

class HostPcmQueue {
public:
    void Attach(PacedWaveOut& paced, AudioActivity& activity);

    void Append16(uint16_t sample);
    void Push();
    void PushBlock(const uint8_t* bytes, uint32_t length);
    void Drop() { bytes_.clear(); }
    void Finish();
    void Abort();

private:
    PacedWaveOut*        paced_    = nullptr;
    AudioActivity*       activity_ = nullptr;
    std::vector<uint8_t> bytes_;
};

}

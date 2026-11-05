#pragma once
#include <cerfcore/core/service.h>
#include <cstdint>
#include <span>

namespace cerfcore {
// Optional board amplifier, applied to copied PCM before it enters the host queue.
class AudioOutputFilter : public Service {
public:
    using Service::Service;
    virtual void ProcessPcm16(std::span<uint8_t> samples) const = 0;
};
}

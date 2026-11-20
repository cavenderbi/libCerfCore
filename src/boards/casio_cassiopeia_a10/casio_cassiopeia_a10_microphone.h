#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class CasioCassiopeiaA10Microphone : public Service {
public:
    using Service::Service;

    virtual void Open(uint32_t rate_hz)                                 = 0;
    virtual void Fill(uint8_t* slots, uint32_t count, uint32_t slot_bytes) = 0;

protected:
    static constexpr uint32_t kSampleBytes = 2u;

    static void Silence(uint8_t* slots, uint32_t count, uint32_t slot_bytes);
};

}

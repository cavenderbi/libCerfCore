#pragma once

#include "capped_byte_fifo.h"
#include "wave_in_request.h"

#include <cerfcore/host/audio_inputs.h>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace cerfcore {

class InstanceThreads;

class WaveInFifo {
public:
    WaveInFifo() = default;
    ~WaveInFifo() { Stop(); }
    WaveInFifo(const WaveInFifo&)            = delete;
    WaveInFifo& operator=(const WaveInFifo&) = delete;

    void   Start(InstanceThreads& threads, AudioInputs* inputs, const char* log_tag, size_t cap_bytes);
    void   Stop();

    void   Request(uint32_t sample_rate_hz, uint16_t channels, uint16_t bits);
    void   Clear() { fifo_.Clear(); }
    size_t Take(uint8_t* dst, size_t bytes) { return fifo_.Take(dst, bytes); }
    size_t TakeIntoSlots(uint8_t* dst, uint32_t slots, uint32_t sample_bytes, uint32_t slot_bytes);

private:
    void Main();

    CappedByteFifo                    fifo_;
    WaveInRequest                     request_;
    std::mutex                        mtx_;
    std::condition_variable           cv_;
    std::unique_ptr<AudioInputDevice> device_;
    bool                              stop_ = false;
    std::thread                       thread_;
};

}

#include "wave_in_fifo.h"

#include <cerfcore/core/instance_threads.h>

#include <algorithm>
#include <vector>

namespace cerfcore {

void WaveInFifo::Start(InstanceThreads& threads, AudioInputs* inputs, const char* log_tag, size_t cap_bytes) {
    fifo_.SetCap(cap_bytes);
    if (inputs == nullptr) return;
    device_ = inputs->CreateInput(log_tag);
    thread_ = threads.Start([this] { Main(); });
}

void WaveInFifo::Stop() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void WaveInFifo::Request(uint32_t sample_rate_hz, uint16_t channels, uint16_t bits) {
    if (!request_.Set(sample_rate_hz, channels, bits)) return;
    std::lock_guard<std::mutex> lk(mtx_);
    cv_.notify_all();
}

size_t WaveInFifo::TakeIntoSlots(uint8_t* dst, uint32_t slots, uint32_t sample_bytes, uint32_t slot_bytes) {
    std::vector<uint8_t> samples(static_cast<size_t>(slots) * sample_bytes);
    const size_t         taken = fifo_.Take(samples.data(), samples.size());
    for (uint32_t i = 0; i < slots; ++i) {
        std::copy_n(samples.data() + static_cast<size_t>(i) * sample_bytes, sample_bytes,
                    dst + static_cast<size_t>(i) * slot_bytes);
    }
    return taken;
}

void WaveInFifo::Main() {
    bool                         open = false;
    std::unique_lock<std::mutex> lk(mtx_);
    while (!stop_) {
        AudioFormat format;
        if (!request_.TakeChange(format)) {
            cv_.wait(lk, [this] { return stop_ || request_.Pending(); });
            continue;
        }
        lk.unlock();
        if (open) device_->Close();
        fifo_.Clear();
        open = device_->Open(format, [this](const uint8_t* bytes, size_t length) { fifo_.Push(bytes, length); });
        lk.lock();
    }
    lk.unlock();
    if (open) device_->Close();
}

}

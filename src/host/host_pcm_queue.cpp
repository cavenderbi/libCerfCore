#include "host_pcm_queue.h"

#include <cerfcore/core/byte_order.h>
#include "audio_activity.h"
#include "paced_wave_out.h"

namespace cerfcore {

void HostPcmQueue::Attach(PacedWaveOut& paced, AudioActivity& activity) {
    paced_    = &paced;
    activity_ = &activity;
}

void HostPcmQueue::Append16(uint16_t sample) { cerf::le::Append16(bytes_, sample); }

void HostPcmQueue::Push() {
    if (bytes_.empty()) return;
    PushBlock(bytes_.data(), static_cast<uint32_t>(bytes_.size()));
    bytes_.clear();
}

void HostPcmQueue::PushBlock(const uint8_t* bytes, uint32_t length) {
    activity_->MarkTx();
    paced_->QueueOutputInHostBlocks(bytes, length);
}

void HostPcmQueue::Finish() {
    Push();
    paced_->FinishAudioOut();
}

void HostPcmQueue::Abort() {
    paced_->StopAudioOut();
    bytes_.clear();
}

}

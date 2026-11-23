#include "sa1111_sac_host_output.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/instance_threads.h>
#include "../../core/board_parts.h"
#include "intel_sa1111_part_id.h"
#include "../../execution/guest_cycle_clock.h"

namespace cerfcore {

namespace {

constexpr uint16_t kChannels       = 2u;
constexpr uint16_t kBitsPerSample  = 16u;
constexpr uint32_t kHostIdleRateHz = 44100u;

}

bool Sa1111SacHostOutput::ShouldRegister() {
    return emu_.Get<BoardParts>().Has(PartId::IntelSa1111);
}

void Sa1111SacHostOutput::OnReady() {
    out_.Start(emu_.Get<Fatal>(), emu_.Get<InstanceThreads>(), emu_.TryGet<AudioOutputs>(), "Sa1111Sac", kHostIdleRateHz, kChannels, kBitsPerSample);
    rate_ = kHostIdleRateHz;
}

void Sa1111SacHostOutput::OnShutdown() { out_.Stop(); }

void Sa1111SacHostOutput::SetRate(uint64_t fs_num, uint64_t fs_den) {
    const uint32_t rate = static_cast<uint32_t>(GuestCycleClock::Rate{fs_num, fs_den}.RoundedHz());
    if (rate == rate_) return;
    rate_ = rate;
    out_.SetFormat(rate, kChannels, kBitsPerSample);
}

void Sa1111SacHostOutput::Begin() {
    if (active_) return;
    out_.BeginAudioOut();
    active_ = true;
}

void Sa1111SacHostOutput::End() {
    if (active_) out_.FinishAudioOut();
    active_ = false;
}

void Sa1111SacHostOutput::Queue(const uint8_t* data, uint32_t bytes) {
    out_.QueueOutput(data, bytes);
}

void Sa1111SacHostOutput::Drop() {
    out_.StopAudioOut();
    active_ = false;
    rate_   = 0u;
}

REGISTER_SERVICE(Sa1111SacHostOutput);

}

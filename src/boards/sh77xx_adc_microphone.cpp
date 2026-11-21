#include "sh77xx_adc_microphone.h"

#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/state/hibernation.h>
#include "../host/audio_activity.h"

namespace cerfcore {

namespace {

constexpr size_t   kMicFifoBytes   = 65536u;
constexpr uint16_t kMicChannels    = 1u;
constexpr uint16_t kMicBits        = 16u;
constexpr int32_t  kPcm16Midpoint  = 0x8000;
constexpr uint32_t kPcm16ToAdShift = 6u;

}

void Sh77xxAdcMicrophone::OnReady() {
    activity_ = &emu_.Get<AudioActivity>();
    activity_->NotePresent();
    mic_.Start(emu_.Get<InstanceThreads>(), emu_.TryGet<AudioInputs>(), "Sh77xxAdcMicrophone", kMicFifoBytes);
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { mic_.Clear(); });
}

void Sh77xxAdcMicrophone::OnShutdown() { mic_.Stop(); }

void Sh77xxAdcMicrophone::SetRate(uint32_t rate_hz) { mic_.Request(rate_hz, kMicChannels, kMicBits); }

uint16_t Sh77xxAdcMicrophone::Convert() {
    uint8_t pcm[2];
    if (mic_.Take(pcm, sizeof(pcm)) != sizeof(pcm)) return static_cast<uint16_t>(kPcm16Midpoint >> kPcm16ToAdShift);
    activity_->MarkRx();
    const int32_t sample = static_cast<int16_t>(cerf::le::U16(pcm, 0u));
    return static_cast<uint16_t>((sample + kPcm16Midpoint) >> kPcm16ToAdShift);
}

}

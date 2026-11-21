#include "sh77xx_dac_speaker.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/state/hibernation.h>
#include "../host/audio_activity.h"
#include "../socs/sh77xx/sh77xx_dac.h"

namespace cerfcore {

namespace {

constexpr uint32_t              kRenderHz       = 44100u;
constexpr GuestCycleClock::Rate kRenderRate{kRenderHz, 1u};
constexpr uint16_t              kRenderChannels = 1u;
constexpr uint16_t              kRenderBits     = 16u;
constexpr int                   kDacMidpoint    = 0x80;
constexpr int                   kDacToPcm16     = 256;

}

void Sh77xxDacSpeaker::OnReady() {
    clock_                  = &emu_.Get<GuestCycleClock>();
    dac_                    = &emu_.Get<Sh77xxDac>();
    AudioActivity& activity = emu_.Get<AudioActivity>();
    activity.NotePresent();
    paced_.Start(emu_.Get<Fatal>(), emu_.Get<InstanceThreads>(), emu_.TryGet<AudioOutputs>(), "Sh77xxDacSpeaker", kRenderHz,
                 kRenderChannels, kRenderBits);
    out_.Attach(paced_, activity);
    samples_.SetRate(clock_->ClockRate(), kRenderRate);
    clock_->RegisterRateListener([this] { samples_.Rescale(clock_->Cycles(), clock_->ClockRate(), kRenderRate); });
    flush_.Attach([this] { Flush(); });
    dac_->AddOutputListener([this] { Update(); });
    dac_->AddRestoreListener([this] { EndStream(); });
    AddSpeakerSwitchListener([this] { Update(); });
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { Update(); });
}

void Sh77xxDacSpeaker::OnShutdown() { paced_.Stop(); }

void Sh77xxDacSpeaker::Update() {
    Render();
    const Sh77xxDac::Output out = dac_->ChannelOutput(DacChannel());
    const bool audible = out.enabled && SpeakerOn();
    level_             = static_cast<int16_t>((static_cast<int>(out.value) - kDacMidpoint) * kDacToPcm16);
    if (audible == audible_) return;
    audible_ = audible;
    if (audible) {
        samples_.Start(clock_->Cycles());
        rendered_ = 0u;
        paced_.BeginAudioOut();
        flush_.Start(kHostPcmFlushRate);
        return;
    }
    flush_.Stop();
    out_.Finish();
}

void Sh77xxDacSpeaker::Render() {
    if (!audible_) return;
    const uint64_t now = samples_.TicksAt(clock_->Cycles());
    for (; rendered_ < now; ++rendered_) out_.Append16(static_cast<uint16_t>(level_));
}

void Sh77xxDacSpeaker::Flush() {
    Render();
    out_.Push();
}

void Sh77xxDacSpeaker::EndStream() {
    flush_.Stop();
    out_.Abort();
    audible_ = false;
}

}

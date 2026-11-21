#include "vr4101_aiu_output.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/host/audio_outputs.h>
#include <cerfcore/state/state_stream.h>
#include "../../host/audio_activity.h"

#include <algorithm>

namespace cerfcore {

namespace {

constexpr int64_t kFullScale = 32767;
constexpr int64_t kMaxVolume = 3;

int64_t Amplitude(uint16_t volume) { return kFullScale * volume / kMaxVolume; }

}

void Vr4101AiuOutput::AddPwm(uint16_t high, uint16_t low, uint16_t volume, uint64_t ticks) {
    const int64_t period = static_cast<int64_t>(high) + low;
    const int64_t level  = period == 0 ? 0 : (2 * static_cast<int64_t>(high) - period) * Amplitude(volume) / period;
    Add(static_cast<int32_t>(level), ticks);
}

void Vr4101AiuOutput::AddSquare(uint64_t origin, uint64_t half, uint16_t volume, uint64_t from, uint64_t to) {
    if (half == 0u) {
        Add(0, to - from);
        return;
    }
    while (from < to) {
        const uint64_t phase = (from - origin) / half;
        const uint64_t edge  = std::min(origin + (phase + 1u) * half, to);
        Add(static_cast<int32_t>((phase & 1u) == 0u ? Amplitude(volume) : -Amplitude(volume)), edge - from);
        from = edge;
    }
}

void Vr4101AiuOutput::Attach(CerfEmulator& emu, GuestCycleClock::Rate tick_rate) {
    window_ = tick_rate.num;
    scale_  = static_cast<uint64_t>(kHostRate) * tick_rate.den;
    paced_.Start(emu.Get<Fatal>(), emu.Get<InstanceThreads>(), emu.TryGet<AudioOutputs>(), "Vr4101Aiu", kHostRate, 1, 16);
    AudioActivity& activity = emu.Get<AudioActivity>();
    activity.NotePresent();
    pcm_.Attach(paced_, activity);
}

void Vr4101AiuOutput::Stop() { paced_.Stop(); }

void Vr4101AiuOutput::Begin() {
    filled_ = 0u;
    area_   = 0;
    pcm_.Drop();
    paced_.SetFormat(kHostRate, 1, 16);
    paced_.BeginAudioOut();
}

void Vr4101AiuOutput::Add(int32_t level, uint64_t ticks) {
    uint64_t left = ticks * scale_;
    while (left != 0u) {
        const uint64_t take = std::min(left, window_ - filled_);
        area_ += static_cast<int64_t>(level) * static_cast<int64_t>(take);
        filled_ += take;
        left -= take;
        if (filled_ == window_) {
            Emit(area_ / static_cast<int64_t>(window_));
            filled_ = 0u;
            area_   = 0;
        }
    }
}

void Vr4101AiuOutput::Emit(int64_t sample) {
    pcm_.Append16(static_cast<uint16_t>(static_cast<int16_t>(std::clamp<int64_t>(sample, -32767, 32767))));
}

void Vr4101AiuOutput::Flush() { pcm_.Push(); }

void Vr4101AiuOutput::Finish() {
    pcm_.Finish();
    filled_ = 0u;
    area_   = 0;
}

void Vr4101AiuOutput::Resume(bool running) {
    pcm_.Abort();
    if (!running) return;
    paced_.SetFormat(kHostRate, 1, 16);
    paced_.BeginAudioOut();
}

void Vr4101AiuOutput::Save(StateWriter& w) const {
    w.Write("out_filled", filled_);
    w.Write("out_area", area_);
}

void Vr4101AiuOutput::Restore(StateReader& r) {
    r.Read("out_filled", filled_);
    r.Read("out_area", area_);
}

}

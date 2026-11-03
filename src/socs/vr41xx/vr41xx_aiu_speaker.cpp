#include "vr41xx_aiu_speaker.h"

#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/state/state_stream.h>
#include "../../host/audio_activity_widget.h"
#include "vr41xx_audio_switch.h"
#include "vr41xx_dcu.h"

namespace cerfcore {

namespace {

constexpr uint16_t kSidleIntr = 0x0002u;
constexpr uint16_t kSIntr     = 0x0004u;
constexpr uint16_t kSendIntr  = 0x0008u;
constexpr uint16_t kSodatv    = 0x0008u;
constexpr uint16_t kSdmav     = 0x0004u;
constexpr uint16_t kDataMask  = 0x03FFu;
constexpr uint16_t kDataReset = 0x0200u;
constexpr int      kMidScale  = 512;

constexpr Vr41xxAiuPathConfig kConfig{Vr41xxAiuChannel::Out, kDmaMskAout, kSIntr, kSendIntr, "spk"};

}

Vr41xxAiuSpeaker::Vr41xxAiuSpeaker(CerfEmulator& emu, Vr41xxAiuPathHost& host)
    : Vr41xxAiuPath(emu, host, kConfig) {}

void Vr41xxAiuSpeaker::AttachHost() {
    Attach();
    switch_    = &emu_.Get<Vr41xxAudioSwitch>();
    switch_on_ = switch_->SpeakerOn();
    paced_.Start(emu_.Get<Fatal>(), emu_.Get<InstanceThreads>(), "Vr41xxAiu", 0, 1, 16);
    emu_.Get<AudioActivityWidget>().NotePresent();
}

void Vr41xxAiuSpeaker::StopHost() { paced_.Stop(); }

void Vr41xxAiuSpeaker::SwitchChanged() {
    CatchUp();
    switch_on_ = switch_->SpeakerOn();
}

uint16_t Vr41xxAiuSpeaker::ValidBits() const {
    return static_cast<uint16_t>((sodatv_ ? kSodatv : 0u) | (sdmav_ ? kSdmav : 0u));
}

void Vr41xxAiuSpeaker::WriteSodat(uint16_t value) { sodat_ = static_cast<uint16_t>(value & kDataMask); }

void Vr41xxAiuSpeaker::WriteSdmadat(uint16_t value) { sdmadat_ = static_cast<uint16_t>(value & kDataMask); }

void Vr41xxAiuSpeaker::WriteValidBits(uint16_t bits) {
    sodatv_ = (bits & kSodatv) != 0u;
    sdmav_  = (bits & kSdmav) != 0u;
    if (sodatv_) primed_ = true;
}

void Vr41xxAiuSpeaker::Convert() {
    int sample = 0;
    if (sodatv_) {
        if (Audible()) sample = (static_cast<int>(sodat_ & kDataMask) - kMidScale) * 64;
        sodatv_ = false;
    } else if (primed_) {
        host_.RaiseAiuInt(kSidleIntr);
    }
    cerf::le::Append16(out_, static_cast<uint16_t>(static_cast<int16_t>(sample)));
    if (sdmav_) {
        sodat_  = sdmadat_;
        sodatv_ = true;
        sdmav_  = false;
    }
    Serve();
}

void Vr41xxAiuSpeaker::Serve() {
    while (!sdmav_ && DmaAllowed()) {
        const uint16_t v = static_cast<uint16_t>(mem_->ReadHalf(TakeDmaAddress()) & kDataMask);
        if (!sodatv_) {
            sodat_  = v;
            sodatv_ = true;
            primed_ = true;
        } else {
            sdmadat_ = v;
            sdmav_   = true;
        }
    }
}

uint64_t Vr41xxAiuSpeaker::ConversionsToNextChange() const {
    if (DmaAllowed()) return HalfwordsToBoundary();
    if (!primed_ || host_.AiuIntPending(kSidleIntr)) return 0u;
    if (!sodatv_) return 1u;
    return sdmav_ ? 3u : 2u;
}

void Vr41xxAiuSpeaker::ClearPipeline() {
    sodat_   = kDataReset;
    sdmadat_ = kDataReset;
    sodatv_  = false;
    sdmav_   = false;
    primed_  = false;
    out_.clear();
}

void Vr41xxAiuSpeaker::OnStart() {
    paced_.SetFormat(RateHz(), 1, 16);
    paced_.BeginAudioOut();
}

void Vr41xxAiuSpeaker::OnStop() {
    FlushHost();
    paced_.FinishAudioOut();
}

void Vr41xxAiuSpeaker::OnRateChanged(uint32_t hz) {
    FlushHost();
    paced_.SetFormat(hz, 1, 16);
}

void Vr41xxAiuSpeaker::FinishCatchUp() { FlushHost(); }

void Vr41xxAiuSpeaker::FlushHost() {
    if (out_.empty()) return;
    emu_.Get<AudioActivityWidget>().MarkTx();
    paced_.QueueOutputInHostBlocks(out_.data(), static_cast<uint32_t>(out_.size()));
    out_.clear();
}

void Vr41xxAiuSpeaker::SavePipeline(StateWriter& w) {
    w.Write("spk_sodat", sodat_);
    w.Write("spk_sdmadat", sdmadat_);
    w.Write<uint8_t>("spk_sodatv", sodatv_ ? 1u : 0u);
    w.Write<uint8_t>("spk_sdmav", sdmav_ ? 1u : 0u);
    w.Write<uint8_t>("spk_primed", primed_ ? 1u : 0u);
    w.Write<uint8_t>("spk_switch_on", switch_on_ ? 1u : 0u);
}

void Vr41xxAiuSpeaker::RestorePipeline(StateReader& r) {
    uint8_t sodatv = 0u, sdmav = 0u, primed = 0u, switch_on = 0u;
    r.Read("spk_sodat", sodat_);
    r.Read("spk_sdmadat", sdmadat_);
    r.Read("spk_sodatv", sodatv);
    r.Read("spk_sdmav", sdmav);
    r.Read("spk_primed", primed);
    r.Read("spk_switch_on", switch_on);
    sodatv_    = sodatv != 0u;
    sdmav_     = sdmav != 0u;
    primed_    = primed != 0u;
    switch_on_ = switch_on != 0u;
    out_.clear();
}

void Vr41xxAiuSpeaker::ResumeHost() {
    paced_.StopAudioOut();
    out_.clear();
    if (!Enabled()) return;
    paced_.SetFormat(RateHz(), 1, 16);
    paced_.BeginAudioOut();
}

}

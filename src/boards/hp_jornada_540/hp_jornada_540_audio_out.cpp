#include "hp_jornada_540_audio_out.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/instance_threads.h>
#include "../../host/audio_activity.h"
#include "../../socs/sh77xx/sh77xx_cpg.h"
#include "../../socs/sh77xx/sh77xx_dmac.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "hp_jornada_540_audio_request_line.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kDreqChannel  = 0u;
constexpr uint8_t  kPortJMute    = 0x10u;
constexpr uint16_t kHostChannels = 2u;
constexpr uint16_t kHostBits     = 16u;

}

bool HpJornada540AudioOut::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

void HpJornada540AudioOut::OnReady() {
    dmac_                   = &emu_.Get<Sh77xxDmac>();
    wiring_                 = &emu_.Get<Sh77xxPortWiring>();
    request_                = &emu_.Get<HpJornada540AudioRequestLine>();
    ckio_hz_                = emu_.Get<Sh77xxCpg>().AttachToCkioPin();
    AudioActivity& activity = emu_.Get<AudioActivity>();
    activity.NotePresent();
    paced_.Start(emu_.Get<Fatal>(), emu_.Get<InstanceThreads>(), emu_.TryGet<AudioOutputs>(), "HpJornada540AudioOut", 0u,
                 kHostChannels, kHostBits);
    out_.Attach(paced_, activity);
    frames_.Attach([this] { Frame(); });
    flush_.Attach([this] { out_.Push(); });
}

void HpJornada540AudioOut::OnShutdown() { paced_.Stop(); }

void HpJornada540AudioOut::SetEnabled(bool enabled) {
    enabled_ = enabled;
    Retime();
    DriveRequest();
}

void HpJornada540AudioOut::SetFifoReset(bool held) {
    reset_held_ = held;
    if (held) ClearFifo();
    DriveRequest();
}

void HpJornada540AudioOut::SetFrameDivisor(uint16_t divisor) {
    divisor_ = divisor;
    Retime();
}

void HpJornada540AudioOut::WriteSample(uint16_t word) {
    if (reset_held_ || Fill() == kFifoWords) {
        emu_.Get<Fatal>().Die("HpJornada540AudioOut: sample word 0x%04X written with %u of %u FIFO words filled, FIFO "
                              "reset %s",
                              word, Fill(), kFifoWords, reset_held_ ? "held" : "released");
    }
    fifo_[written_ % kFifoWords] = word;
    ++written_;
    DriveRequest();
}

void HpJornada540AudioOut::PowerOnReset() {
    enabled_    = false;
    reset_held_ = false;
    divisor_    = 0u;
    ClearFifo();
    Retime();
    DriveRequest();
}

uint32_t HpJornada540AudioOut::Fill() const { return static_cast<uint16_t>(written_ - played_); }

bool HpJornada540AudioOut::Requesting() const { return enabled_ && !reset_held_ && Fill() < kFifoWords; }

bool HpJornada540AudioOut::Running() const { return enabled_ && divisor_ != 0u; }

GuestCycleClock::Rate HpJornada540AudioOut::FrameRate() const { return GuestCycleClock::Rate{ckio_hz_, divisor_}; }

uint32_t HpJornada540AudioOut::FrameHz() const { return static_cast<uint32_t>((ckio_hz_ + divisor_ / 2u) / divisor_); }

bool HpJornada540AudioOut::Muted() const { return (wiring_->NetLevels(kSh77xxPortJ) & kPortJMute) != 0u; }

uint16_t HpJornada540AudioOut::Pop() {
    const uint16_t word = fifo_[played_ % kFifoWords];
    ++played_;
    return word;
}

void HpJornada540AudioOut::ClearFifo() {
    fifo_.fill(0u);
    written_ = 0u;
    played_  = 0u;
}

void HpJornada540AudioOut::DriveRequest() {
    if (request_->Drive(Requesting())) dmac_->DreqChanged(kDreqChannel);
}

void HpJornada540AudioOut::Retime() {
    const bool run = Running();
    if (run && running_) {
        out_.Push();
        frames_.SetRate(FrameRate());
        paced_.SetFormat(FrameHz(), kHostChannels, kHostBits);
        return;
    }
    if (run) {
        running_ = true;
        frames_.Start(FrameRate());
        BeginStream();
        return;
    }
    if (!running_) return;
    running_ = false;
    frames_.Stop();
    flush_.Stop();
    out_.Finish();
}

void HpJornada540AudioOut::BeginStream() {
    paced_.SetFormat(FrameHz(), kHostChannels, kHostBits);
    paced_.BeginAudioOut();
    flush_.Start(kHostPcmFlushRate);
}

void HpJornada540AudioOut::EndStream() {
    flush_.Stop();
    out_.Abort();
}

void HpJornada540AudioOut::Frame() {
    const bool     whole   = Fill() >= kFrameWords;
    const uint16_t right   = whole ? Pop() : 0u;
    const uint16_t left    = whole ? Pop() : 0u;
    const bool     audible = whole && !Muted();
    out_.Append16(audible ? left : 0u);
    out_.Append16(audible ? right : 0u);
    if (whole) DriveRequest();
}

void HpJornada540AudioOut::SaveState(StateWriter& w) const {
    w.Write("audio_fifo", fifo_);
    w.Write("audio_written", written_);
    w.Write("audio_played", played_);
    w.Write("audio_divisor", divisor_);
    frames_.Save(w, "audio_");
}

void HpJornada540AudioOut::RestoreState(StateReader& r, bool enabled, bool reset_held) {
    EndStream();
    r.Read("audio_fifo", fifo_);
    r.Read("audio_written", written_);
    r.Read("audio_played", played_);
    r.Read("audio_divisor", divisor_);
    frames_.Restore(r, "audio_");
    enabled_    = enabled;
    reset_held_ = reset_held;
    running_    = Running();
    if (running_) BeginStream();
    request_->Drive(Requesting());
}

REGISTER_SERVICE(HpJornada540AudioOut);

}

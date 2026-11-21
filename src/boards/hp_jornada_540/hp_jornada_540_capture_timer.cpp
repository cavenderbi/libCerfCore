#include "hp_jornada_540_capture_timer.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_cpg.h"
#include "../sh77xx_microphone_trigger.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kRun            = 0x0008u;
constexpr uint32_t kPrescalerShift = 4u;
constexpr uint16_t kPrescalerMask  = 0x3u;

constexpr uint64_t kPrescalers[4] = {1u, 2u, 8u, 16u};

}

bool HpJornada540CaptureTimer::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

void HpJornada540CaptureTimer::OnReady() {
    trigger_ = &emu_.Get<Sh77xxMicrophoneTrigger>();
    ckio_hz_ = emu_.Get<Sh77xxCpg>().AttachToCkioPin();
    pulses_.Attach([this] { Pulse(); });
}

void HpJornada540CaptureTimer::SetDivisor(uint16_t divisor) {
    divisor_ = divisor;
    Retime();
}

void HpJornada540CaptureTimer::SetControl(uint16_t control) {
    control_ = control;
    Retime();
}

void HpJornada540CaptureTimer::PowerOnReset() {
    divisor_ = 0u;
    control_ = 0u;
    Retime();
}

bool HpJornada540CaptureTimer::Running() const { return (control_ & kRun) != 0u && divisor_ != 0u; }

GuestCycleClock::Rate HpJornada540CaptureTimer::PulseRate() const {
    const uint64_t prescaler = kPrescalers[(control_ >> kPrescalerShift) & kPrescalerMask];
    return GuestCycleClock::Rate{ckio_hz_, prescaler * divisor_};
}

void HpJornada540CaptureTimer::Retime() {
    const bool run = Running();
    if (run && running_) {
        pulses_.SetRate(PulseRate());
    } else if (run) {
        pulses_.Start(PulseRate());
    } else if (running_) {
        pulses_.Stop();
    }
    running_ = run;
}

void HpJornada540CaptureTimer::Pulse() {
    const GuestCycleClock::Rate rate = PulseRate();
    trigger_->Pulse(static_cast<uint32_t>((rate.num + rate.den / 2u) / rate.den));
}

void HpJornada540CaptureTimer::SaveState(StateWriter& w) const {
    w.Write("capture_divisor", divisor_);
    pulses_.Save(w, "capture_");
}

void HpJornada540CaptureTimer::RestoreState(StateReader& r, uint16_t control) {
    r.Read("capture_divisor", divisor_);
    control_ = control;
    running_ = Running();
    pulses_.Restore(r, "capture_");
}

REGISTER_SERVICE(HpJornada540CaptureTimer);

}

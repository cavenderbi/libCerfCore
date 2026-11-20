#include "guest_clock_touch_input.h"

#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

void GuestClockTouchInput::OnReady() {
    samples_.Attach([this] { OnSample(); });
    HostPenTouchInput::OnReady();
}

void GuestClockTouchInput::StartSamples(GuestCycleClock::Rate rate) {
    samples_.Start(rate);
}

void GuestClockTouchInput::SetSampleRate(GuestCycleClock::Rate rate) {
    samples_.SetRate(rate);
}

void GuestClockTouchInput::StopSamples() {
    samples_.Stop();
}

void GuestClockTouchInput::ClearOnRestore() {
    StopSamples();
    ClearPens();
}

}

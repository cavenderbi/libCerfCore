#pragma once

#include "../../execution/guest_cycle_clock.h"
#include "../../socs/guest_sample_clock.h"
#include "host_pen_touch_input.h"

namespace cerfcore {

class GuestClockTouchInput : public HostPenTouchInput {
public:
    using HostPenTouchInput::HostPenTouchInput;

    void OnReady() final;

protected:
    virtual void OnSample() = 0;

    void StartSamples(GuestCycleClock::Rate rate);
    void SetSampleRate(GuestCycleClock::Rate rate);
    void StopSamples();
    void ClearOnRestore();

private:
    GuestSampleClock samples_{emu_};
};

}

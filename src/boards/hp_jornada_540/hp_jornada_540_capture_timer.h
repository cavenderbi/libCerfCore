#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/state/state_stream.h>
#include "../../socs/guest_sample_clock.h"

#include <cstdint>

namespace cerfcore {

class Sh77xxMicrophoneTrigger;

class HpJornada540CaptureTimer : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void SetDivisor(uint16_t divisor);
    void SetControl(uint16_t control);
    void PowerOnReset();

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r, uint16_t control);

private:
    bool                  Running() const;
    GuestCycleClock::Rate PulseRate() const;
    void                  Retime();
    void                  Pulse();

    GuestSampleClock         pulses_{emu_};
    Sh77xxMicrophoneTrigger* trigger_ = nullptr;
    uint64_t                 ckio_hz_ = 0u;
    uint16_t                 divisor_ = 0u;
    uint16_t                 control_ = 0u;
    bool                     running_ = false;
};

}

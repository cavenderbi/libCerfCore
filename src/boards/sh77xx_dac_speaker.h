#pragma once

#include <cerfcore/core/service.h>
#include "../execution/guest_cycle_clock.h"
#include "../host/paced_wave_out.h"
#include "../socs/guest_sample_clock.h"
#include "../socs/rated_tick_count.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace cerfcore {

class AudioActivity;
class Sh77xxDac;

class Sh77xxDacSpeaker : public Service {
public:
    using Service::Service;

    void OnReady() override;
    void OnShutdown() override;

protected:
    virtual uint32_t DacChannel() const                                     = 0;
    virtual bool     SpeakerOn() const                                      = 0;
    virtual void     AddSpeakerSwitchListener(std::function<void()> listener) = 0;

private:
    void Update();
    void Render();
    void Flush();
    void Push();
    void EndStream();

    GuestCycleClock*     clock_    = nullptr;
    Sh77xxDac*           dac_      = nullptr;
    AudioActivity*       activity_ = nullptr;
    PacedWaveOut         paced_;
    RatedTickCount       samples_{emu_};
    GuestSampleClock     flush_{emu_};
    std::vector<uint8_t> out_;
    uint64_t             rendered_ = 0u;
    int16_t              level_    = 0;
    bool                 audible_  = false;
};

}

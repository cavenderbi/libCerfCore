#pragma once

#include "../../execution/guest_cycle_clock.h"
#include "../../host/host_pcm_queue.h"
#include "../../host/paced_wave_out.h"

#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

class Vr4101AiuOutput {
public:
    void Attach(CerfEmulator& emu, GuestCycleClock::Rate tick_rate);
    void Stop();

    void Begin();
    void AddPwm(uint16_t high, uint16_t low, uint16_t volume, uint64_t ticks);
    void AddSquare(uint64_t origin, uint64_t half, uint16_t volume, uint64_t from, uint64_t to);
    void Flush();
    void Finish();
    void Resume(bool running);

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);

private:
    void Add(int32_t level, uint64_t ticks);
    void Emit(int64_t sample);

    static constexpr uint32_t kHostRate = 44100u;

    PacedWaveOut paced_;
    HostPcmQueue pcm_;
    uint64_t     window_ = 1u;
    uint64_t     scale_  = 1u;
    uint64_t     filled_ = 0u;
    int64_t      area_   = 0;
};

}

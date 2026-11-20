#pragma once

#include "../../execution/guest_cycle_clock.h"
#include "vr41xx_rtcx_ticks.h"

#include <cstdint>
#include <functional>
#include <mutex>

namespace cerfcore {

class CerfEmulator;

class Vr41xxRtcxDeadline {
public:
    explicit Vr41xxRtcxDeadline(CerfEmulator& emu) : emu_(emu), rtcx_(emu, Vr41xxRtcxDomain::RtcIcuPmu) {}

    void Attach(std::function<void()> on_expiry);

    void     Start(uint64_t period_ticks);
    void     Restart();
    void     Stop();
    bool     Running();
    uint64_t Elapsed();
    void     Restore(bool running, uint64_t period_ticks, uint64_t elapsed);
    void     Rearm();

private:
    void ArmLocked();
    void OnDeadline();

    CerfEmulator&           emu_;
    std::mutex              mtx_;
    Vr41xxRtcxTicks         rtcx_;
    GuestCycleClock*        clock_   = nullptr;
    GuestCycleClock::Event* event_   = nullptr;
    std::function<void()>   on_expiry_;
    bool                    running_ = false;
    uint64_t                period_  = 0;
    uint64_t                anchor_  = 0;
};

}

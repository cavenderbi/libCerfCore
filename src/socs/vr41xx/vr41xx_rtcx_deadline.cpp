#include "vr41xx_rtcx_deadline.h"

#include <cerfcore/core/cerf_emulator.h>

#include <utility>

namespace cerfcore {

void Vr41xxRtcxDeadline::Attach(std::function<void()> on_expiry) {
    on_expiry_ = std::move(on_expiry);
    clock_     = &emu_.Get<GuestCycleClock>();
    event_     = clock_->Add([this] { OnDeadline(); });
    rtcx_.Attach();
    clock_->RegisterRateListener([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        rtcx_.Rescale();
        ArmLocked();
    });
}

void Vr41xxRtcxDeadline::Start(uint64_t period_ticks) {
    std::lock_guard<std::mutex> lk(mtx_);
    running_ = true;
    period_  = period_ticks;
    anchor_  = rtcx_.Now();
    ArmLocked();
}

void Vr41xxRtcxDeadline::Restart() {
    std::lock_guard<std::mutex> lk(mtx_);
    anchor_ = rtcx_.Now();
    ArmLocked();
}

void Vr41xxRtcxDeadline::Stop() {
    std::lock_guard<std::mutex> lk(mtx_);
    running_ = false;
    clock_->Disarm(event_);
}

bool Vr41xxRtcxDeadline::Running() {
    std::lock_guard<std::mutex> lk(mtx_);
    return running_;
}

uint64_t Vr41xxRtcxDeadline::Elapsed() {
    std::lock_guard<std::mutex> lk(mtx_);
    return running_ ? rtcx_.Now() - anchor_ : 0u;
}

void Vr41xxRtcxDeadline::Restore(bool running, uint64_t period_ticks, uint64_t elapsed) {
    std::lock_guard<std::mutex> lk(mtx_);
    running_ = running;
    period_  = period_ticks;
    rtcx_.Rebase();
    anchor_ = rtcx_.Now() - elapsed;
}

void Vr41xxRtcxDeadline::Rearm() {
    std::lock_guard<std::mutex> lk(mtx_);
    ArmLocked();
}

void Vr41xxRtcxDeadline::ArmLocked() {
    if (!running_) {
        clock_->Disarm(event_);
        return;
    }
    rtcx_.ArmAt(event_, anchor_ + period_);
}

void Vr41xxRtcxDeadline::OnDeadline() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!running_) return;
        if (rtcx_.Now() - anchor_ < period_) {
            ArmLocked();
            return;
        }
        running_ = false;
    }
    on_expiry_();
}

}

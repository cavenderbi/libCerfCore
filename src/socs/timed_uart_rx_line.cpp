#include "timed_uart_rx_line.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/hibernation.h>
#include <cerfengine/host_request_channel.h>
#include "../execution/engine_bridge.h"

#include <utility>

namespace cerfcore {

void TimedUartRxLine::Attach(std::function<void()> on_due, std::function<void()> on_host_request,
                             std::function<void()> on_rate) {
    clock_         = &emu_.Get<GuestCycleClock>();
    event_         = clock_->Add(std::move(on_due));
    host_requests_ = &emu_.Get<EngineBridge>().Engine().Get<cerfengine::HostRequestChannel>();
    host_requests_->RegisterListener(on_host_request);
    emu_.Get<Hibernation>().RegisterPreSaveListener(std::move(on_host_request));
    clock_->RegisterRateListener(std::move(on_rate));
}

bool TimedUartRxLine::Push(const uint8_t* data, size_t n) {
    if (!line_.Push(data, n)) return false;
    host_requests_->Request();
    return true;
}

bool TimedUartRxLine::Intake(const Timing& timing, uint64_t now, bool wanted) {
    switch (line_.TakeIn(timing.clocked, clock_->ClockRate(), timing.bit, timing.frame, now)) {
        case UartRxLine::Intake::kNoClock:
            return line_.Idle();
        case UartRxLine::Intake::kOverflow:
            RateOverflow();
        case UartRxLine::Intake::kQueued:
            Arm(now, wanted);
            return false;
        case UartRxLine::Intake::kNone:
            return false;
    }
    return false;
}

bool TimedUartRxLine::Retime(const Timing& old_timing, const Timing& timing, uint64_t now, bool wanted) {
    if (!line_.Timed()) return false;
    if (!timing.clocked) {
        const bool dropped = line_.DropNoClock();
        clock_->Disarm(event_);
        return dropped && line_.Idle();
    }
    bool moved = false;
    if (!line_.Retime(old_timing.frame, timing.frame, clock_->ClockRate(), timing.bit, now, moved)) RateOverflow();
    Arm(now, wanted);
    return false;
}

void TimedUartRxLine::Rescale(uint64_t now, bool wanted) {
    if (!line_.Rescale(clock_->ClockRate(), now)) RateOverflow();
    Arm(now, wanted);
}

void TimedUartRxLine::Arm(uint64_t now, bool wanted) { line_.ArmEvent(*clock_, event_, now, wanted); }

void TimedUartRxLine::Stop() {
    line_.Reset();
    clock_->Disarm(event_);
}

void TimedUartRxLine::Save(StateWriter& w) const { line_.Save(w, clock_->Cycles()); }

void TimedUartRxLine::Restore(StateReader& r) {
    line_.Restore(r, clock_->ClockRate(), clock_->Cycles());
    clock_->Disarm(event_);
}

void TimedUartRxLine::RateOverflow() const {
    emu_.Get<Fatal>().Die("%s RX: the bit rate overflows the bit counter scale", line_.Name());
}

}

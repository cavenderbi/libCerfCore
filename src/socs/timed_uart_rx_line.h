#pragma once

#include "../execution/guest_cycle_clock.h"
#include "uart_rx_line.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace cerfengine {
class HostRequestChannel;
}

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

class TimedUartRxLine {
public:
    struct Timing {
        bool                  clocked = false;
        GuestCycleClock::Rate bit{};
        UartRxLine::Frame     frame{};
    };

    TimedUartRxLine(CerfEmulator& emu, const char* name) : emu_(emu), line_(name) {}

    void Attach(std::function<void()> on_due, std::function<void()> on_host_request, std::function<void()> on_rate);

    UartRxLine&       Line() { return line_; }
    const UartRxLine& Line() const { return line_; }
    uint64_t          Now() const { return clock_->Cycles(); }

    bool Push(const uint8_t* data, size_t n);

    bool Intake(const Timing& timing, uint64_t now, bool wanted);
    bool Retime(const Timing& old_timing, const Timing& timing, uint64_t now, bool wanted);
    void Rescale(uint64_t now, bool wanted);
    void Arm(uint64_t now, bool wanted);
    void Stop();

    template <typename Land>
    bool Settle(uint64_t now, bool wanted, Land&& land) {
        const bool landed = line_.LandDue(now, land);
        Arm(now, wanted);
        return landed;
    }

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);

private:
    [[noreturn]] void RateOverflow() const;

    CerfEmulator&                   emu_;
    UartRxLine                      line_;
    GuestCycleClock*                clock_         = nullptr;
    GuestCycleClock::Event*         event_         = nullptr;
    cerfengine::HostRequestChannel* host_requests_ = nullptr;
};

}

#pragma once

#include "../../execution/guest_cycle_clock.h"
#include "../timed_uart_rx_line.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace cerfcore {

class CerfEmulator;
class Sh77xxCpg;
class Sh77xxFrqcr;
class StateReader;
class StateWriter;

class Sh77xxSciRxLine {
public:
    explicit Sh77xxSciRxLine(CerfEmulator& emu) : emu_(emu), timed_(emu, "SCI") {}

    void Attach(std::function<void()> on_due, std::function<void()> on_host_request, std::function<void()> on_rate);

    bool Push(const uint8_t* data, size_t n) { return timed_.Push(data, n); }
    bool Empty() const;

    void    Initialize();
    uint8_t Data() const;
    uint8_t Flags() const;
    void    Clear(uint8_t bits);
    void    TakeWire(uint8_t smr, uint8_t brr, bool receiving);
    uint8_t Settle();
    void    SetArmed(bool armed);
    void    Reformat(uint8_t old_smr, uint8_t smr, uint8_t brr);
    void    Rescale();
    void    Stop();

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);

private:
    static UartRxLine::Frame FrameOf(uint8_t smr);

    TimedUartRxLine::Timing TimingOf(uint8_t smr, uint8_t brr) const;
    uint8_t                 Land(uint8_t byte);

    CerfEmulator&   emu_;
    Sh77xxCpg*      cpg_   = nullptr;
    Sh77xxFrqcr*    frqcr_ = nullptr;
    uint8_t         rdr_   = 0u;
    uint8_t         flags_ = 0u;
    bool            armed_ = false;
    TimedUartRxLine timed_;
};

}

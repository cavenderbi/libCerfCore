#pragma once

#include "sh77xx_clock_ticks.h"

#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

class Sh77xxRefreshTimer {
public:
    explicit Sh77xxRefreshTimer(CerfEmulator& emu) : emu_(emu), ckio_(emu, Sh77xxTickedClock::kCkio) {}

    void Attach();
    void PowerOnReset();

    void WriteControl(uint16_t rtcsr);
    void WriteCount(uint16_t rtcnt);
    void WriteConstant(uint16_t rtcor);
    void WriteRefreshCount(uint16_t rfcr);

    uint16_t RefreshCount();

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);
    void PostRestore();

private:
    uint64_t InputTicks() const;
    void     Fold();
    void     CountRefreshRequests(uint64_t requests);

    CerfEmulator&    emu_;
    Sh77xxClockTicks ckio_;
    uint32_t         shift_         = 0u;
    bool             lmts512_       = false;
    uint8_t          rtcor_         = 0u;
    uint8_t          count_         = 0u;
    uint64_t         anchor_        = 0u;
    uint32_t         matches_       = 0u;
    bool             count_known_   = true;
    bool             matches_known_ = true;
};

}

#pragma once

#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

class CompactFlashPowerMode {
public:
    explicit CompactFlashPowerMode(CerfEmulator& emu) : emu_(emu) {}

    void PowerOn();
    void SetDefaults();
    void HostAccess();
    bool Sleeping();
    void Wake() { sleeping_ = false; }
    void Sleep() { sleeping_ = true; }
    void SetIdleTimer(uint8_t count);
    void MarkActivity();

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

private:
    static constexpr int64_t kTimerStepNs = 5000000;

    int64_t NowNs();

    CerfEmulator& emu_;
    bool    sleeping_      = false;
    bool    timer_enabled_ = true;
    int64_t timer_ns_      = kTimerStepNs;
    bool    anchored_      = false;
    int64_t idle_since_ns_ = 0;
};

}

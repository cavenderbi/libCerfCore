#pragma once

#include <cstdint>

namespace cerfcore {

class Vr41xxRtcxTicks;

class Vr41xxPiuSampleSchedule {
public:
    explicit Vr41xxPiuSampleSchedule(Vr41xxRtcxTicks& rtcx) : rtcx_(rtcx) {}

    void     HoldAtNow();
    void     StartAtNow(uint64_t interval);
    bool     Due();
    uint64_t NextCycle();
    uint64_t Take(uint64_t interval);
    bool     SkipLost(uint64_t busy_end, uint64_t interval);
    uint64_t TicksToNext();
    void     SetTicksToNext(uint64_t ticks);

private:
    Vr41xxRtcxTicks& rtcx_;
    uint64_t         next_ = 0;
};

}

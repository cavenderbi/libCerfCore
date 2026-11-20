#include "vr41xx_piu_sample_schedule.h"

#include "vr41xx_rtcx_ticks.h"

#include <algorithm>

namespace cerfcore {

void Vr41xxPiuSampleSchedule::HoldAtNow() { next_ = rtcx_.Now(); }

void Vr41xxPiuSampleSchedule::StartAtNow(uint64_t interval) { next_ = rtcx_.Now() + interval; }

bool Vr41xxPiuSampleSchedule::Due() { return rtcx_.Running() && rtcx_.Now() >= next_; }

uint64_t Vr41xxPiuSampleSchedule::NextCycle() { return rtcx_.CycleOf(next_); }

uint64_t Vr41xxPiuSampleSchedule::Take(uint64_t interval) {
    const uint64_t start = next_;
    next_ += interval;
    return start;
}

bool Vr41xxPiuSampleSchedule::SkipLost(uint64_t busy_end, uint64_t interval) {
    if (rtcx_.CycleOf(next_) >= busy_end) return false;
    const uint64_t step = std::max<uint64_t>(interval, 1u);
    while (rtcx_.CycleOf(next_) < busy_end) next_ += step;
    return true;
}

uint64_t Vr41xxPiuSampleSchedule::TicksToNext() {
    const uint64_t now = rtcx_.Now();
    return next_ > now ? next_ - now : 0u;
}

void Vr41xxPiuSampleSchedule::SetTicksToNext(uint64_t ticks) { next_ = rtcx_.Now() + ticks; }

}

#pragma once

#include "../reload_down_counter.h"

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

class Pr31x00LcdDfSignal {
public:
    void Reset(uint32_t dfval);
    void Start(bool every_frame, uint64_t frame_lines);
    void Restart(uint64_t from_tick, uint64_t frame_lines);
    void Stop(uint64_t tick);
    void SetMode(uint64_t tick, bool every_frame);
    void SetDfVal(uint64_t tick, uint32_t dfval);
    void SetFrameEnd(uint64_t end_tick) { end_ = end_tick; }

    uint64_t TogglesThrough(uint64_t tick) const;
    bool     NextToggle(uint64_t tick, uint64_t& at) const;

    void Save(StateWriter& w, uint64_t tick);
    void Restore(StateReader& r);

private:
    uint64_t EpochToggles(uint64_t tick) const;
    uint64_t FrameStartsThrough(uint64_t tick) const { return tick / frame_ + 1u; }
    uint64_t FirstFrameTick() const { return from_start_ ? epoch_tick_ : epoch_tick_ + 1u; }

    ReloadDownCounter counter_{ReloadDownCounter::FlagPoint::kReload};
    uint64_t          done_        = 0;
    uint64_t          epoch_tick_  = 0;
    uint64_t          epoch_flags_ = 0;
    uint64_t          frame_       = 1;
    uint64_t          end_         = UINT64_MAX;
    bool              every_frame_ = false;
    bool              from_start_  = false;
};

}

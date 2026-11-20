#pragma once

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

class Pr31x00LcdLoads {
public:
    enum class Kind : uint8_t { kNone, kVidRate, kLineVal, kFreeze };

    void Reset();
    void ClearTiming();
    void DueVidRate(uint64_t tick);
    void DueLineVal(uint64_t tick);
    void DueFreeze(uint64_t tick);
    void CancelFreeze() { freeze_due_ = false; }
    void MoveFreeze(uint64_t tick);
    void WriteControl(bool envid, bool freeze_bit);
    void Take(Kind kind);

    bool     FreezeDue() const { return freeze_due_; }
    bool     Frozen() const { return frozen_; }
    uint64_t FrameEnd() const { return freeze_due_ ? freeze_tick_ : UINT64_MAX; }
    Kind     Next(uint64_t& tick) const;

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);

private:
    uint64_t vidrate_tick_ = 0;
    uint64_t lineval_tick_ = 0;
    uint64_t freeze_tick_  = 0;
    bool     vidrate_due_  = false;
    bool     lineval_due_  = false;
    bool     freeze_due_   = false;
    bool     frozen_       = false;
    bool     envid_low_    = false;
};

}

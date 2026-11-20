#include "pr31x00_lcd_loads.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

void Pr31x00LcdLoads::Reset() {
    vidrate_due_ = false;
    lineval_due_ = false;
    freeze_due_  = false;
    frozen_      = false;
    envid_low_   = false;
}

void Pr31x00LcdLoads::ClearTiming() {
    vidrate_due_ = false;
    lineval_due_ = false;
}

void Pr31x00LcdLoads::DueVidRate(uint64_t tick) {
    if (vidrate_due_) return;
    vidrate_due_  = true;
    vidrate_tick_ = tick;
}

void Pr31x00LcdLoads::DueLineVal(uint64_t tick) {
    if (lineval_due_) return;
    lineval_due_  = true;
    lineval_tick_ = tick;
}

void Pr31x00LcdLoads::DueFreeze(uint64_t tick) {
    if (freeze_due_) return;
    freeze_due_  = true;
    freeze_tick_ = tick;
}

void Pr31x00LcdLoads::MoveFreeze(uint64_t tick) {
    if (freeze_due_) freeze_tick_ = tick;
}

void Pr31x00LcdLoads::WriteControl(bool envid, bool freeze_bit) {
    if (!freeze_bit) freeze_due_ = false;
    if (frozen_ && !envid) envid_low_ = true;
    if (frozen_ && envid_low_ && !freeze_bit) {
        frozen_    = false;
        envid_low_ = false;
    }
}

void Pr31x00LcdLoads::Take(Kind kind) {
    switch (kind) {
        case Kind::kVidRate: vidrate_due_ = false; return;
        case Kind::kLineVal: lineval_due_ = false; return;
        case Kind::kFreeze:
            freeze_due_ = false;
            frozen_     = true;
            return;
        case Kind::kNone: return;
    }
}

Pr31x00LcdLoads::Kind Pr31x00LcdLoads::Next(uint64_t& tick) const {
    Kind kind = Kind::kNone;
    tick      = UINT64_MAX;
    if (freeze_due_) {
        kind = Kind::kFreeze;
        tick = freeze_tick_;
    }
    if (lineval_due_ && lineval_tick_ < tick) {
        kind = Kind::kLineVal;
        tick = lineval_tick_;
    }
    if (vidrate_due_ && vidrate_tick_ < tick) {
        kind = Kind::kVidRate;
        tick = vidrate_tick_;
    }
    return kind;
}

void Pr31x00LcdLoads::Save(StateWriter& w) const {
    w.Write<uint8_t>("vidrate_due", vidrate_due_ ? 1u : 0u);
    w.Write("vidrate_tick", vidrate_tick_);
    w.Write<uint8_t>("lineval_due", lineval_due_ ? 1u : 0u);
    w.Write("lineval_tick", lineval_tick_);
    w.Write<uint8_t>("freeze_due", freeze_due_ ? 1u : 0u);
    w.Write("freeze_tick", freeze_tick_);
    w.Write<uint8_t>("freeze_stopped", frozen_ ? 1u : 0u);
    w.Write<uint8_t>("freeze_envid_low", envid_low_ ? 1u : 0u);
}

void Pr31x00LcdLoads::Restore(StateReader& r) {
    uint8_t vidrate_due = 0, lineval_due = 0, freeze_due = 0, frozen = 0, envid_low = 0;
    r.Read("vidrate_due", vidrate_due);
    r.Read("vidrate_tick", vidrate_tick_);
    r.Read("lineval_due", lineval_due);
    r.Read("lineval_tick", lineval_tick_);
    r.Read("freeze_due", freeze_due);
    r.Read("freeze_tick", freeze_tick_);
    r.Read("freeze_stopped", frozen);
    r.Read("freeze_envid_low", envid_low);
    vidrate_due_ = vidrate_due != 0u;
    lineval_due_ = lineval_due != 0u;
    freeze_due_  = freeze_due != 0u;
    frozen_      = frozen != 0u;
    envid_low_   = envid_low != 0u;
}

}

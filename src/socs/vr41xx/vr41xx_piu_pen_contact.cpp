#include "vr41xx_piu_pen_contact.h"

#include <cerfcore/state/state_stream.h>

#include <cstdint>

namespace cerfcore {

void Vr41xxPiuPenContact::Edge(bool down, bool penchg_pending) {
    prev_ = cur_;
    cur_  = down;
    Latch(penchg_pending);
}

void Vr41xxPiuPenContact::Latch(bool penchg_pending) {
    if (!held_ || !penchg_pending) stc_ = cur_;
}

void Vr41xxPiuPenContact::PenChgCleared() {
    if (held_) stc_ = cur_;
}

void Vr41xxPiuPenContact::Reset() {
    prev_ = false;
    stc_  = cur_;
}

void Vr41xxPiuPenContact::Save(StateWriter& w) const {
    w.Write<uint8_t>("pen_prev", prev_ ? 1 : 0);
    w.Write<uint8_t>("penstc", stc_ ? 1 : 0);
}

void Vr41xxPiuPenContact::Restore(StateReader& r, bool penchg_pending) {
    uint8_t prev = 0, stc = 0;
    r.Read("pen_prev", prev);
    r.Read("penstc", stc);
    cur_  = false;
    prev_ = prev != 0;
    stc_  = stc != 0;
    Latch(penchg_pending);
}

}

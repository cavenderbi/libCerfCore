#pragma once

namespace cerfcore {

class StateReader;
class StateWriter;

class Vr41xxPiuPenContact {
public:
    explicit Vr41xxPiuPenContact(bool stc_held_by_penchg) : held_(stc_held_by_penchg) {}

    bool Down() const { return cur_; }
    bool Previous() const { return prev_; }
    bool Stc() const { return stc_; }

    void Edge(bool down, bool penchg_pending);
    void PenChgCleared();
    void Reset();

    void Save(StateWriter& w) const;
    void Restore(StateReader& r, bool penchg_pending);

private:
    void Latch(bool penchg_pending);

    bool held_;
    bool cur_  = false;
    bool prev_ = false;
    bool stc_  = false;
};

}

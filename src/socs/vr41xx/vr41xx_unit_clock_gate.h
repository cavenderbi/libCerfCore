#pragma once

#include <cstdint>
#include <functional>

namespace cerfengine { class MipsInterruptChannel; }

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;
class Vr41xxCmu;

class Vr41xxUnitClockGate {
public:
    explicit Vr41xxUnitClockGate(CerfEmulator& emu) : emu_(emu) {}

    void Attach(uint16_t cmu_mask, std::function<void()> on_change);
    void Refresh();
    bool Running() const { return supplied_ && !suspended_; }

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);

private:
    CerfEmulator&                     emu_;
    Vr41xxCmu*                        cmu_     = nullptr;
    cerfengine::MipsInterruptChannel* channel_ = nullptr;
    std::function<void()>             on_change_;
    uint16_t                          mask_      = 0u;
    bool                              supplied_  = false;
    bool                              suspended_ = false;
};

}

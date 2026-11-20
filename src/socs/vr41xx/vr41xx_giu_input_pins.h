#pragma once

#include <cstdint>
#include <optional>

namespace cerfcore {

class StateReader;
class StateWriter;

struct Vr41xxGiuPinEdge {
    bool prev  = false;
    bool level = false;
};

class Vr41xxGiuInputPins {
public:
    explicit Vr41xxGiuInputPins(uint16_t pull_pins) : pull_pins_(pull_pins) {}

    uint32_t Levels() const;
    uint32_t Established() const;
    bool     Level(int pin) const;
    uint32_t Merged(uint32_t driven_out, uint32_t outputs) const;

    Vr41xxGiuPinEdge                Drive(int pin, bool level);
    std::optional<Vr41xxGiuPinEdge> Release(int pin);

    uint16_t UseUpdn() const { return useupdn_; }
    uint16_t TermUpdn() const { return termupdn_; }
    void     SetUseUpdn(uint16_t value) { useupdn_ = value; }
    void     SetTermUpdn(uint16_t value) { termupdn_ = value; }
    void     ResetPulls();

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);

private:
    uint16_t pull_pins_;
    uint32_t level_    = 0u;
    uint32_t driven_   = 0u;
    uint16_t useupdn_  = 0u;
    uint16_t termupdn_ = 0u;
};

}

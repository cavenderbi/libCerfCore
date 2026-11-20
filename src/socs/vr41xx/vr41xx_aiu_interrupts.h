#pragma once

#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;
class Vr41xxIcu;

class Vr41xxAiuInterrupts {
public:
    explicit Vr41xxAiuInterrupts(CerfEmulator& emu) : emu_(emu) {}

    void Attach();

    uint16_t Bits() const { return bits_; }
    bool     Pending(uint16_t mask) const { return (bits_ & mask) == mask; }

    void     Raise(uint16_t bits);
    uint16_t Clear(uint16_t bits);
    void     Reset();
    void     Publish();

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);

private:
    CerfEmulator& emu_;
    Vr41xxIcu*    icu_  = nullptr;
    uint16_t      bits_ = 0u;
};

}

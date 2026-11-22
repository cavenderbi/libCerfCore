#pragma once

#include <cstdint>
#include <span>

namespace cerfcore {

class StateReader;
class StateWriter;

class EpsonLutTable {
public:
    void     Rgb(uint32_t index, uint8_t& r4, uint8_t& g4, uint8_t& b4) const;
    uint32_t Xrgb(uint32_t index, bool color) const;
    void Store(uint8_t index, const uint8_t (&rgb)[3]);
    uint8_t Component(uint8_t index, uint8_t component) const { return entries_[index][component]; }

    void SaveState(StateWriter& w, const char* name) const;
    void RestoreState(StateReader& r, const char* name);

private:
    uint8_t entries_[256][3] = {};
};

class EpsonLutPort {
public:
    uint8_t Read(const EpsonLutTable& table);
    void    Write(uint8_t value, std::span<EpsonLutTable* const> targets);
    void    PointAt(uint8_t index);
    void    ResetPointer();

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    void Step();

    uint8_t index_        = 0;
    uint8_t component_    = 0;
    uint8_t rgb_latch_[2] = {};
};

}

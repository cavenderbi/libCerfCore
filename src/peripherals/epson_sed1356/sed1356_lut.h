#pragma once

#include "../epson_lut/epson_lut.h"

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

class Sed1356Lut {
public:
    uint8_t Read(uint32_t select);
    void    Write(uint32_t select, uint8_t value);
    void    PointAt(uint8_t index) { port_.PointAt(index); }
    void    ResetPointer() { port_.ResetPointer(); }
    void    LcdRgb(uint32_t index, uint8_t& r4, uint8_t& g4, uint8_t& b4) const { lcd_.Rgb(index, r4, g4, b4); }

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    EpsonLutTable lcd_;
    EpsonLutTable crt_;
    EpsonLutPort  port_;
};

}

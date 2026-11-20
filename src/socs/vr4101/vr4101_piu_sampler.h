#pragma once

#include "../vr41xx/vr41xx_piu_host_pen.h"

#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class Vr4101PiuInputs;

class Vr4101PiuSampler {
public:
    struct Datum {
        uint16_t ain;
        uint16_t pencont;
    };

    explicit Vr4101PiuSampler(CerfEmulator& emu) : emu_(emu) {}

    void Attach(uint64_t tclk_num, uint64_t tclk_den);

    uint16_t SetLength(uint16_t state, uint16_t cnt) const;
    uint64_t CoordinateSetTclocks(uint16_t stbl, uint16_t cnt) const;
    Datum    SetDatum(uint16_t state, uint16_t cnt, uint16_t cmd, uint16_t index) const;
    uint16_t Convert(const Datum& d, uint16_t stbl, const Vr41xxPiuPenPoint& pen);

private:
    CerfEmulator&    emu_;
    Vr4101PiuInputs* inputs_           = nullptr;
    uint64_t         conversion_ticks_ = 0u;
};

}

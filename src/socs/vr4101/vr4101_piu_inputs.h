#pragma once

#include "../vr41xx/vr41xx_piu_host_pen.h"
#include "../vr41xx/vr41xx_piu_screen.h"

#include <cstdint>

namespace cerfcore {

class Vr4101PiuInputs : public Vr41xxPiuScreen {
public:
    using Vr41xxPiuScreen::Vr41xxPiuScreen;

    virtual uint32_t ConverterBits() const = 0;

    virtual uint16_t Convert(uint16_t ain, uint16_t pencont, const Vr41xxPiuPenPoint& pen) = 0;
};

}

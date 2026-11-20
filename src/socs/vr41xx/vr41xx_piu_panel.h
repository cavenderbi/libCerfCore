#pragma once

#include "vr41xx_piu_screen.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

class Vr41xxPiuPanel : public Vr41xxPiuScreen {
public:
    using Vr41xxPiuScreen::Vr41xxPiuScreen;

    /* PIUPBn4REG carries Z (touch pressure) (VR4121 UM Table 20-4, VR4102 UM Table 19-4);
       std::nullopt leaves its D15 VALID clear ("0: Invalid", VR4121 UM 20.3.9). */
    virtual std::optional<uint16_t> PressureSample(bool contact) = 0;

    virtual uint16_t ReleasedPlateSample() = 0;

    virtual std::optional<uint16_t> PortSample(uint16_t port) = 0;
};

}

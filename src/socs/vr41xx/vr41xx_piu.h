#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>

namespace cerfcore {

class Vr41xxPiu : public Peripheral {
public:
    using Peripheral::Peripheral;

    /* PADDATA(9:0) is the A/D converter's 10-bit sampling data (VR4121 UM 20.3.9,
       VR4102 UM 19.3.9). */
    virtual void SetPen(bool down, uint16_t pos_x, uint16_t pos_y) = 0;

    virtual void SyntheticTap(uint16_t pos_x, uint16_t pos_y) = 0;
};

}

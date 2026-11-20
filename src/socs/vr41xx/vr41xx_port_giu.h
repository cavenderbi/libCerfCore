#pragma once

#include "vr41xx_giu.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

class Vr41xxPortGiu : public Vr41xxGiu {
public:
    using Vr41xxGiu::Vr41xxGiu;

    virtual uint16_t OutputDataLow() = 0;
    virtual void     RegisterOutputDataLowListener(std::function<void(uint16_t previous)> fn) = 0;

    virtual uint16_t ReadPullRegister(uint32_t off)                  = 0;
    virtual void     WritePullRegister(uint32_t off, uint16_t value) = 0;
};

}

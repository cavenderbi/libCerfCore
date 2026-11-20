#pragma once

#include "vr41xx_piu.h"

#include <cstdint>

namespace cerfcore {

class Vr41xxBufferedPiu : public Vr41xxPiu {
public:
    using Vr41xxPiu::Vr41xxPiu;

    virtual uint32_t Piu2Base() const = 0;
    virtual uint32_t Piu2Size() const = 0;
};

}

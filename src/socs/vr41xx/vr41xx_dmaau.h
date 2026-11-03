#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>

namespace cerfcore {

enum class Vr41xxAiuChannel : uint8_t { In, Out };

class Vr41xxDmaau : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual uint32_t Base(Vr41xxAiuChannel channel) const               = 0;
    virtual uint32_t Address(Vr41xxAiuChannel channel) const            = 0;
    virtual void     SetAddress(Vr41xxAiuChannel channel, uint32_t pa)  = 0;
};

}

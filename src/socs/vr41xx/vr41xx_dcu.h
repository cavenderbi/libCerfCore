#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>

namespace cerfcore {

inline constexpr uint16_t kDmaMskAout = 1u << 2;
inline constexpr uint16_t kDmaMskAin  = 1u << 3;

class Vr41xxDcu : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual bool DmaEnabled(uint16_t msk_bit) const = 0;
};

}

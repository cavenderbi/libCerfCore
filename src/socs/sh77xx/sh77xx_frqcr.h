#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>
#include <functional>

namespace cerfcore {

inline constexpr uint16_t kSh77xxFrqcrStc         = 0x8030u;
inline constexpr uint16_t kSh77xxFrqcrPllCircuit1 = 0x80F0u;

class Sh77xxFrqcr : public Peripheral {
public:
    using Peripheral::Peripheral;

    using ChangeListener = std::function<void(uint16_t from, uint16_t to)>;

    virtual uint16_t Value() const                                = 0;
    virtual void     RegisterChangeListener(ChangeListener listener) = 0;
};

}

#pragma once

#include <cstdint>

namespace cerfcore {

struct Vr41xxDmaPageInterrupts {
    uint16_t one_page;
    uint16_t two_page;

    constexpr uint16_t AtBoundary(uint16_t pending) const {
        return static_cast<uint16_t>(one_page | ((pending & one_page) != 0u ? two_page : 0u));
    }
};

}

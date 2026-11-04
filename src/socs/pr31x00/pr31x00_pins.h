#pragma once

#include <cstdint>

namespace cerfcore {

struct Pr31x00Pins {
    uint32_t level;
    uint32_t driven;
    uint32_t pulled_down;

    uint32_t Low() const;
};

}

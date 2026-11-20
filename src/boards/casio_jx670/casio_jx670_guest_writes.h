#pragma once

#include <cstdint>

namespace cerfcore {

constexpr bool SetsOnly(uint32_t value, uint32_t bits) {
    return (value & ~bits) == 0u;
}

}

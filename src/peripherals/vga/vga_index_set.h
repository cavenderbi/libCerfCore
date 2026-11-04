#pragma once

#include <cstdint>
#include <initializer_list>

namespace cerfcore {

class VgaIndexSet {
public:
    struct Range {
        uint8_t first;
        uint8_t last;
    };

    constexpr VgaIndexSet(std::initializer_list<Range> ranges) {
        for (const Range& r : ranges) {
            for (uint32_t i = r.first; i <= r.last; ++i) bits_[i >> 5] |= 1u << (i & 31u);
        }
    }

    constexpr bool Has(uint8_t index) const { return ((bits_[index >> 5] >> (index & 31u)) & 1u) != 0u; }

private:
    uint32_t bits_[8] = {};
};

}

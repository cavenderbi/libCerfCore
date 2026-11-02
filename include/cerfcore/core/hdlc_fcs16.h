#pragma once

#include <cstddef>
#include <cstdint>

namespace cerfcore {

class HdlcFcs16 {
public:
    static constexpr uint16_t kInit = 0xFFFFu;
    static constexpr uint16_t kGood = 0xF0B8u;

    static uint16_t Update(uint16_t fcs, uint8_t byte);
    static uint16_t Update(uint16_t fcs, const uint8_t* data, size_t len);
};

}

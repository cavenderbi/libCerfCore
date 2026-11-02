#pragma once

#include <cstddef>
#include <cstdint>

namespace cerfcore {

class Crc32 {
public:
    static constexpr uint32_t kGood = 0x2144DF1Cu;

    static uint32_t Update(uint32_t crc, uint8_t byte);
    static uint32_t Update(uint32_t crc, const uint8_t* data, size_t len);
};

}

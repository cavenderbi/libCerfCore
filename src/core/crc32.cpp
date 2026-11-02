#include <cerfcore/core/crc32.h>

namespace cerfcore {

namespace {

constexpr uint32_t kReflectedPolynomial = 0xEDB88320u;

}

uint32_t Crc32::Update(uint32_t crc, uint8_t byte) {
    crc = ~crc ^ byte;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1u) ? kReflectedPolynomial : 0u);
    return ~crc;
}

uint32_t Crc32::Update(uint32_t crc, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) crc = Update(crc, data[i]);
    return crc;
}

}

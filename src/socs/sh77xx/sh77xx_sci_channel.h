#pragma once

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kSh77xxSciBase = 0x1FFFFE80u;

inline constexpr uint8_t kSh77xxSciTdre = 0x80u;
inline constexpr uint8_t kSh77xxSciRdrf = 0x40u;
inline constexpr uint8_t kSh77xxSciOrer = 0x20u;
inline constexpr uint8_t kSh77xxSciTend = 0x04u;

}

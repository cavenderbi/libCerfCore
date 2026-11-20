#pragma once

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kSh77xxArea3PaBase    = 0x0C000000u;
inline constexpr uint32_t kSh77xxArea3Bytes     = 64u << 20;
inline constexpr uint32_t kSh77xxRasWindowBytes = kSh77xxArea3Bytes / 2u;
inline constexpr uint32_t kSh77xxRas3lPaBase    = kSh77xxArea3PaBase;
inline constexpr uint32_t kSh77xxRas3uPaBase    = kSh77xxArea3PaBase + kSh77xxRasWindowBytes;

}

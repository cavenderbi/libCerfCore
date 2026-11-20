#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

namespace cerfcore {

namespace BoardConfigParamId {

inline constexpr std::string_view Ram = "ram";

}

inline constexpr std::pair<std::string_view, uint32_t> kRamBytes[] = {
    {"2mb", 2u << 20},
    {"3mb", 3u << 20},
    {"4mb", 4u << 20},
    {"5mb", 5u << 20},
    {"6mb", 6u << 20},
    {"7mb", 7u << 20},
    {"8mb", 8u << 20},
    {"12mb", 12u << 20},
    {"16mb", 16u << 20},
    {"32mb", 32u << 20},
    {"64mb", 64u << 20},
};

}

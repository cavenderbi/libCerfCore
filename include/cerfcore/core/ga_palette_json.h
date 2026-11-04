#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cerfcore {

inline constexpr size_t kGaPaletteMaxColors = 256;

template <class JsonT>
std::optional<std::vector<uint32_t>> GaPaletteFromJson(const JsonT& value) {
    if (!value.is_array() || value.empty() || value.size() > kGaPaletteMaxColors) return std::nullopt;
    std::vector<uint32_t> colors;
    colors.reserve(value.size());
    for (const auto& color : value) {
        if (!color.is_array() || color.size() != 3) return std::nullopt;
        uint32_t rgb = 0;
        for (const auto& channel : color) {
            if (!channel.is_number_integer()) return std::nullopt;
            const int64_t level = channel.template get<int64_t>();
            if (level < 0 || level > 255) return std::nullopt;
            rgb = (rgb << 8) | static_cast<uint32_t>(level);
        }
        colors.push_back(rgb);
    }
    return colors;
}

template <class JsonT>
JsonT GaPaletteToJson(std::span<const uint32_t> colors) {
    JsonT out = JsonT::array();
    for (const uint32_t rgb : colors)
        out.push_back(JsonT::array({(rgb >> 16) & 0xFFu, (rgb >> 8) & 0xFFu, rgb & 0xFFu}));
    return out;
}

}

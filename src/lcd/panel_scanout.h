#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cerfcore {

enum class PanelPixelFormat : uint8_t {
    kRgb565Le,
};

enum class PanelGrayLayout : uint8_t {
    kPacked2Msb,
    kPlanes2Lsb,
};

using PanelGrayLevels = std::array<uint8_t, 4>;

struct PanelSurface {
    const uint8_t* fb     = nullptr;
    uint32_t       stride = 0u;
    uint32_t       width  = 0u;
    uint32_t       height = 0u;
};

class PanelScanout {
public:
    explicit PanelScanout(PanelPixelFormat format) : format_(format) {}
    PanelScanout(PanelGrayLayout layout, const PanelGrayLevels& levels)
        : gray_(true), layout_(layout), levels_(levels) {}

    void Blit(const PanelSurface& src, uint32_t* dib,
              uint32_t host_w, uint32_t host_h) const;

private:
    uint32_t GrayIndex(const uint8_t* line, size_t plane_bytes, uint32_t x) const;

    bool             gray_   = false;
    PanelPixelFormat format_ = PanelPixelFormat::kRgb565Le;
    PanelGrayLayout  layout_ = PanelGrayLayout::kPacked2Msb;
    PanelGrayLevels  levels_{};
};

}

#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

struct Gpu2dCopySpec;

inline constexpr uint32_t kG2d8888Rgba = 13u;

constexpr uint32_t Imx51Gpu2dFillColor(uint32_t color, uint32_t format) {
    if (format == kG2d8888Rgba) return (color << 8) | (color >> 24);
    return color;
}

constexpr uint32_t Imx51Gpu2dCopyColor(uint32_t color, uint32_t source_format) {
    return source_format == kG2d8888Rgba ? (color >> 8) | (color << 24) : color;
}

uint32_t Imx51Gpu2dSrgbToLinear(uint32_t color);
uint32_t Imx51Gpu2dLinearToSrgb(uint32_t color);

/* The g12 direct-2D BitBlt engine: decodes a solid rect fill or a SCOORD1
   source->dest rect copy from the g12 register file (a render path distinct from
   the VGV2 polygon rasterizer) and drives the rasterizer's pixel writers. Reads
   the register file the command engine owns; holds no state of its own. */
class Imx51Gpu2dDirect2d : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    /* Solid rect fill under G2D_INPUT 0x01/0x09 (fires on the G2D_COLOR latch). */
    void Fill(const uint32_t (&regs)[0x100]);
    /* SCOORD1 source->dest rect copy under G2D_INPUT 0x00/0x02 (fires on the G2D_SXY
       write); `sxy` is the transient trigger value (source origin). */
    void Copy(const uint32_t (&regs)[0x100], uint32_t sxy);

private:
    /* Gates shared by Fill and Copy (ALPHABLEND/ROP/gradient/MASK/dest
       format/BASE0); each op adds its own distinct gates on top. */
    void CheckCommonGates(const uint32_t (&regs)[0x100], bool mask_to_alpha = false, bool dest_swap_rb = false) const;
    /* Copy ARGB8888 or RGBA8888 pixels to ARGB8888, clipped to the destination. */
    void CopyMask(const uint32_t (&regs)[0x100], uint32_t sxy);
    void CopyRect(const Gpu2dCopySpec& c);
    void UnpremultiplyRect(const Gpu2dCopySpec& c);
    [[noreturn]] void Halt(const uint32_t (&regs)[0x100], const char* why,
                           uint32_t addr, uint32_t data) const;
};

}

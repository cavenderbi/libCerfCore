#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

enum class Gpu2dWrap : uint32_t { Clamp = 0, Repeat = 1, Mirror = 2, Border = 3 };

constexpr bool Gpu2dTextureFormat1(uint32_t format) { return format == 0u || format == 1u; }

constexpr bool Gpu2dTextureFormat4(uint32_t format) { return format == 2u; }

constexpr bool Gpu2dTextureFormat8(uint32_t format) {
    return format == 3u || format == 14u;
}

constexpr bool Gpu2dTextureFormat16(uint32_t format) {
    return format == 4u || format == 5u || format == 6u || format == 11u || format == 12u;
}

constexpr uint32_t Gpu2dExpandTextureChannel(uint32_t value, uint32_t maximum) {
    return (value * 255u + maximum / 2u) / maximum;
}

constexpr uint32_t Gpu2dDecodeTexture16(uint16_t pixel, uint32_t format) {
    if (format == 4u) {
        return (((pixel >> 12) & 15u) * 17u << 24) | (((pixel >> 8) & 15u) * 17u << 16) |
            (((pixel >> 4) & 15u) * 17u << 8) | ((pixel & 15u) * 17u);
    }
    if (format == 5u) {
        return ((pixel & 0x8000u ? 255u : 0u) << 24) |
            (Gpu2dExpandTextureChannel((pixel >> 10) & 31u, 31u) << 16) |
            (Gpu2dExpandTextureChannel((pixel >> 5) & 31u, 31u) << 8) |
            Gpu2dExpandTextureChannel(pixel & 31u, 31u);
    }
    if (format == 6u) {
        return 0xFF000000u | (Gpu2dExpandTextureChannel((pixel >> 11) & 31u, 31u) << 16)
            | (Gpu2dExpandTextureChannel((pixel >> 5) & 63u, 63u) << 8)
            | Gpu2dExpandTextureChannel(pixel & 31u, 31u);
    }
    if (format == 11u) {
        return (Gpu2dExpandTextureChannel(pixel & 15u, 15u) << 24)
            | (Gpu2dExpandTextureChannel((pixel >> 12) & 15u, 15u) << 16)
            | (Gpu2dExpandTextureChannel((pixel >> 8) & 15u, 15u) << 8)
            | Gpu2dExpandTextureChannel((pixel >> 4) & 15u, 15u);
    }
    return ((pixel & 1u ? 255u : 0u) << 24)
        | (Gpu2dExpandTextureChannel((pixel >> 11) & 31u, 31u) << 16)
        | (Gpu2dExpandTextureChannel((pixel >> 6) & 31u, 31u) << 8)
        | Gpu2dExpandTextureChannel((pixel >> 1) & 31u, 31u);
}

constexpr uint16_t Gpu2dEncodeTexture16(uint32_t argb, uint32_t format) {
    if (format == 4u) return static_cast<uint16_t>(((argb >> 16) & 0xF000u) |
        ((argb >> 12) & 0x0F00u) | ((argb >> 8) & 0x00F0u) | ((argb >> 4) & 0x000Fu));
    if (format == 5u) return static_cast<uint16_t>(((argb >> 16) & 0x8000u) |
        ((argb >> 9) & 0x7C00u) | ((argb >> 6) & 0x03E0u) | ((argb >> 3) & 0x001Fu));
    if (format == 6u) return static_cast<uint16_t>(((argb >> 8) & 0xF800u) |
        ((argb >> 5) & 0x07E0u) | ((argb >> 3) & 0x001Fu));
    if (format == 11u) return static_cast<uint16_t>(((argb >> 20) & 15u) << 12 |
        ((argb >> 12) & 15u) << 8 | ((argb >> 4) & 15u) << 4 | (argb >> 28));
    return static_cast<uint16_t>(((argb >> 19) & 31u) << 11 |
        ((argb >> 11) & 31u) << 6 | ((argb >> 3) & 31u) << 1 | (argb >> 31));
}

/* GRADW paint input decoded from the g12 register file. The paint colour at a
   dest pixel is the fmt7 texel at the (OUTX,OUTY) the GRADW ISA program computes. */
struct Gpu2dGradwPaint {
    uint32_t texbase;     /* GRADW TEXBASE (texel-atlas / ramp base; gpuaddr==physical) */
    int32_t  texw, texh;  /* GRADW TEXSIZE WIDTH[10:0] / HEIGHT[21:11] (texels) */
    uint32_t format = 7u;
    uint32_t tstride;     /* GRADW TEXCFG.STRIDE + 1 (texels per row) */
    bool     srgb = false;
    bool     swap_rb = false;
    bool     bilinear;    /* TEXCFG.BILIN: 4-tap linear filter vs nearest */
    bool     premultiply; /* TEXCFG.PREMULTIPLY: texel stored premultiplied (else premult the result) */
    Gpu2dWrap wrap_u = Gpu2dWrap::Clamp;
    Gpu2dWrap wrap_v = Gpu2dWrap::Clamp;
    uint32_t border_color = 0;
    uint32_t foreground_color = 0xFFFFFFFFu, background_color = 0u;
    uint32_t inst[8];     /* GRADW ISA words INST0..INST(ninst-1) */
    uint32_t ninst;       /* GRADIENT.INSTRUCTIONS(2) + 1 */
    float    cnst[12];    /* coefficient registers C0-C11 (g12-half-float-unpacked) */
};

class Imx51Gpu2dGradwSampler : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    /* Run the program at BASE0-local pixel (px,py) -> (OUTX,OUTY) texcoords, then
       fetch the texture texel with the selected filtering and wrap modes. */
    uint32_t Sample(const Gpu2dGradwPaint& p, int32_t px, int32_t py) const;
    uint32_t SampleTexel(const Gpu2dGradwPaint& p, int32_t px, int32_t py) const;

private:
    void EvalProgram(const Gpu2dGradwPaint& p, int32_t px, int32_t py,
                     float& outx, float& outy) const;
    uint32_t Fetch(const Gpu2dGradwPaint& p, int32_t tx, int32_t ty) const;
};

}

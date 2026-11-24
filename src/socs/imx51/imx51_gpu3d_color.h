#pragma once

#include <array>
#include <cstdint>

namespace cerfcore {

inline bool Imx51Gpu3dSupportedColorFormat(uint32_t format) {
    return format == 0u || format == 2u || format == 4u || format == 5u;
}

// Measured SYNC2 GSL GMEM-save view: upper 64 KiB, unchanged 16-bit stride.
// A resolve writes system memory and does not change the GMEM binding.
inline bool Imx51Gpu3dRg88UpperBankResolve(uint32_t previous, uint32_t previous_pitch,
    uint32_t current, uint32_t current_pitch, bool resolve) {
    return resolve && previous == 0x00000002u && previous_pitch == 256u &&
        current == 0x00010004u && current_pitch == 256u;
}

// A2xx COLORFORMATX: COLORX_8_8 = 4, two 8-bit unsigned-normalized lanes.
// Code Aurora a2xx_reg.h (9bbf61339d31), adreno_drawctxt.c format tables.
inline std::array<float, 4> Imx51Gpu3dUnpackRg88(uint16_t packed) {
    return {float(packed & 255u) / 255.0f, float(packed >> 8) / 255.0f, 0.0f, 1.0f};
}

// Color mask bits select shader R/G lanes. B/A have no stored bits.
inline uint16_t Imx51Gpu3dPackRg88(uint8_t red, uint8_t green, uint16_t previous, uint32_t mask) {
    const uint16_t low = (mask & 1u) ? red : (previous & 255u);
    const uint16_t high = (mask & 2u) ? green : (previous >> 8);
    return static_cast<uint16_t>(low | (high << 8));
}

}

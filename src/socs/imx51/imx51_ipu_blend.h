#pragma once

#include <cerfcore/core/byte_order.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>
#include <cstdint>

#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif

namespace cerfcore::imx51_ipu {

inline uint32_t BlendPixel(uint32_t fg, uint32_t bg) {
    const uint32_t a = fg >> 24;
    auto ch = [&](uint32_t sh) {
        return (a * ((fg >> sh) & 255u) + (255u - a) * ((bg >> sh) & 255u) + 127u) / 255u;
    };
    return lcd_pixel::PackXrgb(ch(16), ch(8), ch(0));
}

#if defined(__SSE2__) || defined(_M_X64)
inline __m128i BlendPair(__m128i fg, __m128i bg) {
    const __m128i a = _mm_shufflehi_epi16(_mm_shufflelo_epi16(fg, 255), 255);
    const __m128i ia = _mm_sub_epi16(_mm_set1_epi16(255), a);
    __m128i n = _mm_add_epi16(_mm_mullo_epi16(fg, a), _mm_mullo_epi16(bg, ia));
    n = _mm_add_epi16(n, _mm_set1_epi16(128));
    return _mm_srli_epi16(_mm_add_epi16(n, _mm_srli_epi16(n, 8)), 8);
}
#endif

inline void BlendRow(uint32_t* out, const uint8_t* fg, const uint8_t* bg, uint32_t width) {
    uint32_t x = 0u;
#if defined(__SSE2__) || defined(_M_X64)
    const __m128i zero = _mm_setzero_si128();
    const __m128i alpha = _mm_slli_epi32(_mm_set1_epi32(255), 24);
    for (; width - x >= 4u; x += 4u) {
        const __m128i f = _mm_loadu_si128(reinterpret_cast<const __m128i*>(fg + x * 4u));
        __m128i b = zero;
        if (bg != nullptr) {
            const __m128i p = _mm_unpacklo_epi16(
                _mm_loadl_epi64(reinterpret_cast<const __m128i*>(bg + x * 2u)), zero);
            __m128i r = _mm_srli_epi32(p, 11);
            r = _mm_or_si128(_mm_slli_epi32(r, 3), _mm_srli_epi32(r, 2));
            __m128i g = _mm_and_si128(_mm_srli_epi32(p, 5), _mm_set1_epi32(63));
            g = _mm_or_si128(_mm_slli_epi32(g, 2), _mm_srli_epi32(g, 4));
            __m128i blue = _mm_and_si128(p, _mm_set1_epi32(31));
            blue = _mm_or_si128(_mm_slli_epi32(blue, 3), _mm_srli_epi32(blue, 2));
            b = _mm_or_si128(_mm_or_si128(_mm_slli_epi32(r, 16), _mm_slli_epi32(g, 8)), blue);
        }
        const __m128i lo = BlendPair(_mm_unpacklo_epi8(f, zero), _mm_unpacklo_epi8(b, zero));
        const __m128i hi = BlendPair(_mm_unpackhi_epi8(f, zero), _mm_unpackhi_epi8(b, zero));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + x),
                        _mm_or_si128(_mm_packus_epi16(lo, hi), alpha));
    }
#endif
    for (; x < width; ++x) {
        const uint32_t b = bg ? lcd_pixel::Expand565(cerf::le::U16(bg, x * 2u)) : 0xFF000000u;
        out[x] = BlendPixel(cerf::le::U32(fg, x * 4u), b);
    }
}

}

#pragma once

#include <cstdint>

namespace cerfcore {

namespace CerfVirt {

class BltIndexConverter {
public:
    static constexpr uint32_t kMaxColors = 256u;

    void     Build(const uint32_t* colorrefs, uint32_t count);
    uint32_t Convert(uint32_t colorref);
    uint32_t Nearest(uint32_t colorref) const;
    uint32_t ColorOf(uint32_t index) const { return palette_[index & (kMaxColors - 1u)]; }

    static uint32_t ColorRefOf(uint32_t pixel, const uint32_t masks[3], const uint32_t shifts[3]);

private:
    static constexpr int32_t  kBuckets   = 55;
    static constexpr uint32_t kCacheBits = 10u;
    static constexpr uint32_t kCacheSize = 1u << kCacheBits;

    static int32_t  Key(uint32_t colorref);
    static uint32_t Distance(uint32_t a, uint32_t b);
    uint32_t        Search(uint32_t colorref) const;

    uint32_t palette_[kMaxColors]      = {};
    int16_t  next_[kMaxColors]         = {};
    int16_t  head_[kBuckets]           = {};
    uint32_t count_                    = 0;
    int32_t  low_                      = 0;
    int32_t  high_                     = 0;
    uint32_t cache_key_[kCacheSize]    = {};
    uint32_t cache_stamp_[kCacheSize]  = {};
    uint8_t  cache_index_[kCacheSize]  = {};
    uint32_t stamp_                    = 0;
};

}

}

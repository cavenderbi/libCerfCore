#include "cerf_virt_blt_index_converter.h"

#include <algorithm>
#include <iterator>

namespace cerfcore {

namespace CerfVirt {

int32_t BltIndexConverter::Key(uint32_t colorref) {
    return (int32_t)(((colorref & 0xFFFFu) >> 11) + ((colorref >> 21) & 7u) + ((colorref & 0xFFu) >> 4));
}

uint32_t BltIndexConverter::Distance(uint32_t a, uint32_t b) {
    const int32_t red   = (int32_t)(a & 0xFFu) - (int32_t)(b & 0xFFu);
    const int32_t green = (int32_t)((a >> 8) & 0xFFu) - (int32_t)((b >> 8) & 0xFFu);
    const int32_t blue  = (int32_t)((a >> 16) & 0xFFu) - (int32_t)((b >> 16) & 0xFFu);
    return (uint32_t)(red * red + green * green + blue * blue);
}

uint32_t BltIndexConverter::ColorRefOf(uint32_t pixel, const uint32_t masks[3], const uint32_t shifts[3]) {
    uint32_t colorref = 0u;
    for (int c = 0; c < 3; ++c) {
        const uint32_t level = shifts[c] >= 32u ? 0u : ((pixel & masks[c]) << shifts[c]) >> 24;
        colorref |= level << (8u * (uint32_t)c);
    }
    return colorref;
}

void BltIndexConverter::Build(const uint32_t* colorrefs, uint32_t count) {
    if (count == count_ && std::equal(colorrefs, colorrefs + count, palette_)) return;
    count_ = count;
    low_   = kMaxColors;
    high_  = 0;
    std::fill(std::begin(head_), std::end(head_), (int16_t)-1);
    std::fill(std::begin(palette_) + count, std::end(palette_), 0u);
    for (uint32_t i = 0; i < count; ++i) {
        palette_[i]       = colorrefs[i];
        const int32_t key = Key(palette_[i]);
        next_[i]          = head_[key];
        head_[key]        = (int16_t)i;
        high_             = std::max(high_, key);
        low_              = std::min(low_, key);
    }
    int16_t following = head_[high_];
    head_[++high_]    = -1;
    for (int32_t bucket = high_ - 2; bucket >= low_; --bucket) {
        if (head_[bucket] == -1) {
            head_[bucket] = following;
            continue;
        }
        int16_t tail = head_[bucket];
        while (next_[tail] != -1) tail = next_[tail];
        next_[tail] = following;
        following   = head_[bucket];
    }
    if (++stamp_ == 0u) {
        std::fill(std::begin(cache_stamp_), std::end(cache_stamp_), 0u);
        stamp_ = 1u;
    }
}

uint32_t BltIndexConverter::Search(uint32_t colorref) const {
    const int32_t key = Key(colorref);
    int32_t       lo  = (key >= low_ + 4) ? key - 4 : low_;
    int32_t       hi  = (key <= high_ - 5) ? key + 5 : high_;
    if (head_[lo] == head_[hi]) {
        lo = low_;
        hi = high_;
    }
    uint32_t best  = 0x10000000u;
    uint32_t index = 0u;
    for (int16_t entry = head_[lo]; entry != head_[hi]; entry = next_[entry]) {
        const uint32_t error = Distance(colorref, palette_[entry]);
        if (error < best) {
            best  = error;
            index = (uint32_t)entry;
        }
    }
    return index;
}

uint32_t BltIndexConverter::Nearest(uint32_t colorref) const {
    uint32_t best  = Distance(colorref, palette_[0]);
    uint32_t index = 0u;
    for (uint32_t i = 1u; i < count_; ++i) {
        const uint32_t error = Distance(colorref, palette_[i]);
        if (error < best) {
            best  = error;
            index = i;
        }
    }
    return index;
}

uint32_t BltIndexConverter::Convert(uint32_t colorref) {
    const uint32_t slot = (colorref * 2654435761u) >> (32u - kCacheBits);
    if (cache_stamp_[slot] == stamp_ && cache_key_[slot] == colorref) return cache_index_[slot];
    const uint32_t index = Search(colorref);
    cache_stamp_[slot] = stamp_;
    cache_key_[slot]   = colorref;
    cache_index_[slot] = (uint8_t)index;
    return index;
}

}

}

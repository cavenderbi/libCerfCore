#pragma once

#include <cstdint>

namespace cerfcore {

inline uint32_t Rop3(uint8_t rop, uint32_t p, uint32_t s, uint32_t d) {
    uint32_t r = 0u;
    if (rop & 0x01u) r |= ~p & ~s & ~d;
    if (rop & 0x02u) r |= ~p & ~s &  d;
    if (rop & 0x04u) r |= ~p &  s & ~d;
    if (rop & 0x08u) r |= ~p &  s &  d;
    if (rop & 0x10u) r |=  p & ~s & ~d;
    if (rop & 0x20u) r |=  p & ~s &  d;
    if (rop & 0x40u) r |=  p &  s & ~d;
    if (rop & 0x80u) r |=  p &  s &  d;
    return r;
}

inline bool Rop3ReadsPattern(uint8_t rop) {
    return ((rop >> 4) & 0x0Fu) != (rop & 0x0Fu);
}

inline bool Rop3ReadsSource(uint8_t rop) {
    return ((rop >> 2) & 0x33u) != (rop & 0x33u);
}

}

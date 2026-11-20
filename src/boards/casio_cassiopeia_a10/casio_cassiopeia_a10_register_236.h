#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10Register236     = 0x10000236u;
inline constexpr uint16_t kCasioCassiopeiaA10Register236Bits = 0x0001u;

class CasioCassiopeiaA10Register236
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10Register236, kCasioCassiopeiaA10Register236Bits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;
};

}

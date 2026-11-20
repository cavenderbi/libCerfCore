#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10WakeEnables    = 0x10000000u;
inline constexpr uint16_t kCasioCassiopeiaA10WakeEnableBits = 0xFFFFu;

class CasioCassiopeiaA10WakeEnables
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10WakeEnables, kCasioCassiopeiaA10WakeEnableBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;
};

}

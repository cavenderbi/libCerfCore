#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10InputChangeEnables = 0x10000262u;
inline constexpr uint16_t kCasioCassiopeiaA10InputChangeBits    = 0x0003u;

class CasioCassiopeiaA10InputChangeEnables
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10InputChangeEnables,
                                                  kCasioCassiopeiaA10InputChangeBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;
};

}

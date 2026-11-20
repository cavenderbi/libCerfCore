#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10PowerControl     = 0x10000008u;
inline constexpr uint16_t kCasioCassiopeiaA10PowerControlBits = 0x01FFu;

class CasioCassiopeiaA10PowerControl
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10PowerControl, kCasioCassiopeiaA10PowerControlBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;
};

}

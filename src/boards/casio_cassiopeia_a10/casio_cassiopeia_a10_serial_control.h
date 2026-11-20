#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10SerialControl     = 0x10000230u;
inline constexpr uint16_t kCasioCassiopeiaA10SerialControlBits = 0x00EFu;

class CasioCassiopeiaA10SerialControl
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10SerialControl, kCasioCassiopeiaA10SerialControlBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;
};

}

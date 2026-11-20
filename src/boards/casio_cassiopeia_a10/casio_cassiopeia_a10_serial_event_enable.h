#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10SerialEventEnable = 0x100000B6u;
inline constexpr uint16_t kCasioCassiopeiaA10LineChangeEnable  = 0x0001u;
inline constexpr uint16_t kCasioCassiopeiaA10LineFallingEdge   = 0x0004u;

class CasioCassiopeiaA10SerialEventEnable
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10SerialEventEnable,
                                                  kCasioCassiopeiaA10LineChangeEnable |
                                                      kCasioCassiopeiaA10LineFallingEdge> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;
};

}

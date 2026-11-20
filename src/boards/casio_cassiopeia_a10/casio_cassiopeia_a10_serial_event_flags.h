#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10SerialEventFlags = 0x100000B8u;
inline constexpr uint16_t kCasioCassiopeiaA10LineChangeFlag   = 0x0001u;

class CasioCassiopeiaA10SerialEventFlags
    : public CasioCassiopeiaA10FlagsChipRegister<kCasioCassiopeiaA10SerialEventFlags, kCasioCassiopeiaA10LineChangeFlag> {
public:
    using CasioCassiopeiaA10FlagsChipRegister::CasioCassiopeiaA10FlagsChipRegister;
};

}

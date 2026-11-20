#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10BaudDivisor = 0x10000232u;

using CasioCassiopeiaA10BaudDivisor = CasioCassiopeiaA10StoredChipRegister<kCasioCassiopeiaA10BaudDivisor, 0u>;

}

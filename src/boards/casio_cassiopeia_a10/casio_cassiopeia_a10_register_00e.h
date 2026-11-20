#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10Register00E   = 0x1000000Eu;
inline constexpr uint16_t kCasioCassiopeiaA10WakeLatchNmi = 0x0004u;

class CasioCassiopeiaA10Register00E : public CasioCassiopeiaA10StoredChipRegister<kCasioCassiopeiaA10Register00E, 0u> {
public:
    using CasioCassiopeiaA10StoredChipRegister::CasioCassiopeiaA10StoredChipRegister;

protected:
    bool Accepts(uint16_t value) const override;
};

}

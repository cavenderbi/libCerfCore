#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10LedControl = 0x1000027Au;

class CasioCassiopeiaA10LedControl : public CasioCassiopeiaA10StoredChipRegister<kCasioCassiopeiaA10LedControl, 0u> {
public:
    using CasioCassiopeiaA10StoredChipRegister::CasioCassiopeiaA10StoredChipRegister;

    bool ShouldRegister() override;

    bool Blinking() const;
    bool Steady() const;

protected:
    bool Accepts(uint16_t value) const override;
};

}

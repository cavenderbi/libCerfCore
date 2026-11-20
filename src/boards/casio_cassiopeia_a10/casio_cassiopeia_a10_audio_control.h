#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10AudioControl     = 0x10000100u;
inline constexpr uint16_t kCasioCassiopeiaA10AudioControlBits = 0x003Fu;

class CasioCassiopeiaA10AudioControl
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10AudioControl, kCasioCassiopeiaA10AudioControlBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;

    bool MicrophoneOn() const;
};

}

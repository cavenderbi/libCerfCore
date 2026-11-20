#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10AmpControl     = 0x10000102u;
inline constexpr uint16_t kCasioCassiopeiaA10AmpControlBits = 0x0005u;

class CasioCassiopeiaA10AmpControl
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10AmpControl, kCasioCassiopeiaA10AmpControlBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;

    bool SpeakerOn() const;
    bool MicrophoneOn() const;
};

}

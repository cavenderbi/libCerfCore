#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRegister     = 0x10000012u;
constexpr uint16_t kSciOwnedFlag = 0x0040u;

class CasioCassiopeiaA10Register012 : public CasioCassiopeiaA10MaskedChipRegister<kRegister, kSciOwnedFlag> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Register012);

}

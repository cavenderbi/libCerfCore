#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRegister     = 0x10000238u;
constexpr uint16_t kWritableBits = 0x0003u;

class CasioCassiopeiaA10Register238 : public CasioCassiopeiaA10MaskedChipRegister<kRegister, kWritableBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Register238);

}

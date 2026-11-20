#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRegister = 0x10000240u;
constexpr uint16_t kZero     = 0x0000u;

class CasioCassiopeiaA10Register240 : public CasioCassiopeiaA10ChipRegister<kRegister, kZero> {
public:
    using CasioCassiopeiaA10ChipRegister::CasioCassiopeiaA10ChipRegister;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Register240);

}

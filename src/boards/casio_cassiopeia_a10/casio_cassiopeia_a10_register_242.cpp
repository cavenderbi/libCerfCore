#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRegister = 0x10000242u;
constexpr uint16_t kAllOnes  = 0xFFFFu;

class CasioCassiopeiaA10Register242 : public CasioCassiopeiaA10ChipRegister<kRegister, kAllOnes> {
public:
    using CasioCassiopeiaA10ChipRegister::CasioCassiopeiaA10ChipRegister;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Register242);

}

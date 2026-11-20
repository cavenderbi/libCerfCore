#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kStore     = 0x10000006u;
constexpr uint16_t kStoreBits = 0x00FFu;

class CasioCassiopeiaA10StatusStore : public CasioCassiopeiaA10MaskedChipRegister<kStore, kStoreBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10StatusStore);

}

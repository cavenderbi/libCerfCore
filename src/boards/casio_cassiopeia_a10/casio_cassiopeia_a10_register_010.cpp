#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRegister   = 0x10000010u;
constexpr uint16_t kResetValue = 0x0000u;

constexpr uint16_t kGuestValues[] = {0x0000u, 0x0014u, 0x0094u, 0x5000u};

class CasioCassiopeiaA10Register010
    : public CasioCassiopeiaA10ListedChipRegister<kRegister, kResetValue, kGuestValues> {
public:
    using CasioCassiopeiaA10ListedChipRegister::CasioCassiopeiaA10ListedChipRegister;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Register010);

}

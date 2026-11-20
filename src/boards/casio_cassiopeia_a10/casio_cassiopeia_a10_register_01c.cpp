#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRegister = 0x1000001Cu;
constexpr uint16_t kFlagBit  = 0x0080u;

class CasioCassiopeiaA10Register01C : public CasioCassiopeiaA10FlagsChipRegister<kRegister, kFlagBit> {
public:
    using CasioCassiopeiaA10FlagsChipRegister::CasioCassiopeiaA10FlagsChipRegister;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Register01C);

}

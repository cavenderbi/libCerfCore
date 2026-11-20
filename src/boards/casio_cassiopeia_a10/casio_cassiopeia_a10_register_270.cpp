#include "casio_cassiopeia_a10_chip_register.h"

#include "casio_cassiopeia_a10_members.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRegister  = 0x10000270u;
constexpr uint16_t kLevelBits = 0x000Fu;

class CasioCassiopeiaA10Register270 : public CasioCassiopeiaA10MaskedChipRegister<kRegister, kLevelBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;

    bool ShouldRegister() override {
        return CasioCassiopeiaA10MaskedChipRegister::ShouldRegister() &&
               CasioCassiopeiaA10IsA51Class(emu_.Get<BoardContext>().GetBoardId());
    }
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Register270);

}

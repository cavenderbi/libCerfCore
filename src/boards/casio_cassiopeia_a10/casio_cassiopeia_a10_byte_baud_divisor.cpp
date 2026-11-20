#include "casio_cassiopeia_a10_baud_divisor.h"

#include "casio_cassiopeia_a10_members.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint16_t kDivisorBits = 0x00FFu;

class CasioCassiopeiaA10ByteBaudDivisor
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10BaudDivisor, kDivisorBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister::CasioCassiopeiaA10MaskedChipRegister;

    bool ShouldRegister() override {
        return CasioCassiopeiaA10MaskedChipRegister::ShouldRegister() &&
               !CasioCassiopeiaA10IsA51Class(emu_.Get<BoardContext>().GetBoardId());
    }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10ByteBaudDivisor, CasioCassiopeiaA10BaudDivisor);

}

#include "casio_cassiopeia_a10_chip_register.h"

#include "casio_cassiopeia_a10_members.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRegister           = 0x10000272u;
constexpr uint16_t kAmxEightBitColumns = 0x0000u;

class CasioCassiopeiaA10Register272 : public CasioCassiopeiaA10ChipWord<kRegister> {
public:
    using CasioCassiopeiaA10ChipWord::CasioCassiopeiaA10ChipWord;

    bool ShouldRegister() override {
        return CasioCassiopeiaA10ChipWord::ShouldRegister() &&
               CasioCassiopeiaA10IsA51Class(emu_.Get<BoardContext>().GetBoardId());
    }

    uint16_t ReadHalf(uint32_t) override { return kAmxEightBitColumns; }
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Register272);

}

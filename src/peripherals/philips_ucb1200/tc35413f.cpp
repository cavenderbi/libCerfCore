#include "ucb1x00_codec.h"

#include "../../core/board_parts.h"
#include "toshiba_tc35413f_part_id.h"
#include <cerfcore/core/cerf_emulator.h>

#include <cstdint>

namespace cerfcore {

namespace {

/* NetBSD hpcmips ucb1200reg.h TC35413F_ID 0x9712 (TOSHIBA); ucb1200.c ucb_id[]
   drives the TC35413F with the UCB1200 register map. */
constexpr uint16_t kIdTc35413f = 0x9712u;

class Tc35413f : public Ucb1x00Codec {
public:
    using Ucb1x00Codec::Ucb1x00Codec;

    bool ShouldRegister() override {
        return emu_.Get<BoardParts>().Has(PartId::ToshibaTc35413f);
    }

protected:
    uint16_t DeviceId() const override { return kIdTc35413f; }

    std::array<uint16_t, 16> PowerOnRegs() const override { return {}; }
};

}

REGISTER_SERVICE_AS(Tc35413f, Ucb1x00Codec);

}

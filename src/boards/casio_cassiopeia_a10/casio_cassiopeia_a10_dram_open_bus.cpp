#include "../dram_bank_open_bus.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_area3_dram.h"
#include "casio_cassiopeia_a10_family_id.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

namespace {

class CasioCassiopeiaA10DramOpenBus : public DramBankOpenBus {
public:
    using DramBankOpenBus::DramBankOpenBus;

protected:
    uint32_t         BankPaBase() const override { return kSh77xxArea3PaBase; }
    uint32_t         BankBytes() const override { return kSh77xxArea3Bytes; }
    std::string_view WindowBoardFamilyId() const override { return BoardFamilyId::CasioCassiopeiaA10; }
    const char*      WindowTag() const override { return "CasioCassiopeiaA10DramOpenBus"; }
};

}

REGISTER_SERVICE(CasioCassiopeiaA10DramOpenBus);

}

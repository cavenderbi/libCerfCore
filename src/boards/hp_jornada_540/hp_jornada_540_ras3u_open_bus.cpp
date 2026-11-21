#include "../dram_bank_open_bus.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_area3_dram.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

class HpJornada540Ras3uOpenBus : public DramBankOpenBus {
public:
    using DramBankOpenBus::DramBankOpenBus;

protected:
    uint32_t         BankPaBase() const override { return kSh77xxRas3uPaBase; }
    uint32_t         BankBytes() const override { return kSh77xxRasWindowBytes; }
    std::string_view WindowBoardFamilyId() const override { return BoardFamilyId::HpJornada540; }
    const char*      WindowTag() const override { return "HpJornada540Ras3uOpenBus"; }
};

}

REGISTER_SERVICE(HpJornada540Ras3uOpenBus);

}

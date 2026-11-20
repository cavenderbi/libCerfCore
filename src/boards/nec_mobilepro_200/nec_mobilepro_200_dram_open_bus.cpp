#include "../dram_bank_open_bus.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/vr4101/vr4101_dram_area.h"
#include "nec_mobilepro_200_family_id.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

namespace {

class NecMobilepro200DramOpenBus : public DramBankOpenBus {
public:
    using DramBankOpenBus::DramBankOpenBus;

protected:
    uint32_t         BankPaBase() const override { return kVr4101DramPaBase; }
    uint32_t         BankBytes() const override { return kVr4101DramBytes; }
    std::string_view WindowBoardFamilyId() const override { return BoardFamilyId::NecMobilepro200; }
    const char*      WindowTag() const override { return "NecMobilepro200DramOpenBus"; }
};

}

REGISTER_SERVICE(NecMobilepro200DramOpenBus);

}

#include "../dram_bank_open_bus.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../core/board_config.h"
#include "../../socs/sh77xx/sh77xx_area2_dram.h"
#include "../dram_card_param_id.h"
#include "casio_cassiopeia_a10_family_id.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

namespace {

class CasioCassiopeiaA10Area2OpenBus : public DramBankOpenBus {
public:
    using DramBankOpenBus::DramBankOpenBus;

protected:
    uint32_t         BankPaBase() const override { return kSh77xxArea2PaBase; }
    uint32_t         BankBytes() const override { return kSh77xxArea2Bytes; }
    std::string_view WindowBoardFamilyId() const override { return BoardFamilyId::CasioCassiopeiaA10; }
    bool             WindowOnBoard(const BoardContext& bd) const override {
        return emu_.Get<BoardConfig>().Has(BoardConfigParamId::DramCard) && DramBankOpenBus::WindowOnBoard(bd);
    }
    const char* WindowTag() const override { return "CasioCassiopeiaA10Area2OpenBus"; }
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Area2OpenBus);

}

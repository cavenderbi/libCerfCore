#include "hp_600_dram_bank_open_bus.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_area3_dram.h"
#include "hp_600_family_id.h"

namespace cerfcore {

uint32_t Hp600DramBankOpenBus::BankBytes() const { return kSh77xxRasWindowBytes; }

std::string_view Hp600DramBankOpenBus::WindowBoardFamilyId() const { return BoardFamilyId::Hp600; }

bool Hp600DramBankOpenBus::WindowOnBoard(const BoardContext& bd) const {
    return bd.GetSocId() == ProbingSocId() && DramBankOpenBus::WindowOnBoard(bd);
}

}

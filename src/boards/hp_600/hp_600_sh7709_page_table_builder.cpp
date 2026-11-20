#include "../unmapped_window_dram_rom_page_table_builder.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7709/sh7709_id.h"
#include "../../socs/sh77xx/sh77xx_area3_dram.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

class Hp600Sh7709PageTableBuilder : public UnmappedWindowDramRomPageTableBuilder {
public:
    using UnmappedWindowDramRomPageTableBuilder::UnmappedWindowDramRomPageTableBuilder;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7709;
    }

    void OnReady() override {
        SetRamParamDram(kSh77xxRas3lPaBase, kSh77xxRasWindowBytes, DramRepeat::kPowerOfTwoSize);
        UnmappedWindowDramRomPageTableBuilder::OnReady();
    }
};

}

REGISTER_SERVICE_AS(Hp600Sh7709PageTableBuilder, PageTableBuilder);

}

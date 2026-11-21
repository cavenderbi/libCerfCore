#include "../unmapped_window_dram_rom_page_table_builder.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_area3_dram.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

class HpJornada540PageTableBuilder : public UnmappedWindowDramRomPageTableBuilder {
public:
    using UnmappedWindowDramRomPageTableBuilder::UnmappedWindowDramRomPageTableBuilder;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

    void OnReady() override {
        SetRamParamDram(kSh77xxRas3lPaBase, kSh77xxRasWindowBytes, DramRepeat::kPowerOfTwoSize);
        UnmappedWindowDramRomPageTableBuilder::OnReady();
    }
};

}

REGISTER_SERVICE_AS(HpJornada540PageTableBuilder, PageTableBuilder);

}

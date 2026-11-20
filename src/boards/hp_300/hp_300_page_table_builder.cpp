#include "../unmapped_window_dram_rom_page_table_builder.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_area3_dram.h"
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

class Hp300PageTableBuilder : public UnmappedWindowDramRomPageTableBuilder {
public:
    using UnmappedWindowDramRomPageTableBuilder::UnmappedWindowDramRomPageTableBuilder;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override {
        SetRamParamDram(kSh77xxArea3PaBase, kSh77xxArea3Bytes, DramRepeat::kPowerOfTwoSize);
        UnmappedWindowDramRomPageTableBuilder::OnReady();
    }
};

}

REGISTER_SERVICE_AS(Hp300PageTableBuilder, PageTableBuilder);

}

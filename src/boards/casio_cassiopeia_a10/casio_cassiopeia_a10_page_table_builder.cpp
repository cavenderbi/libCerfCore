#include "../unmapped_window_dram_rom_page_table_builder.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../core/board_config.h"
#include "../../socs/sh77xx/sh77xx_area2_dram.h"
#include "../dram_card_param_id.h"
#include "../../socs/sh77xx/sh77xx_area3_dram.h"
#include "casio_cassiopeia_a10_family_id.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10PageTableBuilder : public UnmappedWindowDramRomPageTableBuilder {
public:
    using UnmappedWindowDramRomPageTableBuilder::UnmappedWindowDramRomPageTableBuilder;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        SetRamParamDram(kSh77xxArea3PaBase, kSh77xxArea3Bytes, DramRepeat::kNever);
        if (emu_.Get<BoardConfig>().Has(BoardConfigParamId::DramCard)) {
            SetDramCardBank(kSh77xxArea2PaBase, kSh77xxArea2Bytes);
        }
        UnmappedWindowDramRomPageTableBuilder::OnReady();
    }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10PageTableBuilder, PageTableBuilder);

}

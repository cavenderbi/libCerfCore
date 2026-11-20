#include "../unmapped_window_dram_rom_page_table_builder.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/vr4101/vr4101_dram_area.h"
#include "nec_mobilepro_200_family_id.h"

namespace cerfcore {

namespace {

class NecMobilepro200PageTableBuilder : public UnmappedWindowDramRomPageTableBuilder {
public:
    using UnmappedWindowDramRomPageTableBuilder::UnmappedWindowDramRomPageTableBuilder;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro200;
    }

    void OnReady() override {
        SetRamParamDram(kVr4101DramPaBase, kVr4101DramBytes, DramRepeat::kNever);
        UnmappedWindowDramRomPageTableBuilder::OnReady();
    }
};

}

REGISTER_SERVICE_AS(NecMobilepro200PageTableBuilder, PageTableBuilder);

}

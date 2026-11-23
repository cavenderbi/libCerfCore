#include "../unmapped_window_page_table_builder.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/memory_kind.h>
#include "../../core/board_config.h"
#include "../../socs/sh7729/sh7729_id.h"
#include "../../socs/sh77xx/sh77xx_area3_dram.h"
#include "../ram_param_id.h"
#include "hp_600_family_id.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

namespace {

constexpr uint32_t kBank0Bytes = 16u << 20;

constexpr UnmappedWindowDram kBank0 = UnmappedWindowPageTableBuilder::CachedWindowDram(
    kSh77xxRas3lPaBase, kBank0Bytes, kSh77xxRasWindowBytes, kSh77xxRasWindowBytes);

class Hp600Sh7729PageTableBuilder : public UnmappedWindowPageTableBuilder {
public:
    using UnmappedWindowPageTableBuilder::UnmappedWindowPageTableBuilder;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7729;
    }

    void OnReady() override {
        const uint32_t ram = emu_.Get<BoardConfig>().SelectedBytes(BoardConfigParamId::Ram);
        bank1_ = CachedWindowDram(kSh77xxRas3uPaBase, ram - kBank0Bytes, kSh77xxRasWindowBytes, kSh77xxRasWindowBytes);
        rom_   = PlaceEntryXipRom(kBank0.region.va_base, kBank0Bytes);
    }

    std::vector<DramRegion> CachedDramRegions() const override {
        std::vector<DramRegion> r;
        kBank0.AddCachedTo(r);
        bank1_.AddCachedTo(r);
        return r;
    }

    std::vector<BackedRegion> BackedMemoryRegions() const override {
        std::vector<BackedRegion> r;
        kBank0.AddBackedTo(r);
        bank1_.AddBackedTo(r);
        r.push_back({ rom_.va_base, rom_.pa_base, rom_.size, MemoryKind::kRom });
        return r;
    }

private:
    UnmappedWindowDram bank1_{};
    DramRegion         rom_{};
};

}

REGISTER_SERVICE_AS(Hp600Sh7729PageTableBuilder, PageTableBuilder);

}

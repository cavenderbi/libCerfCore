#include "../unmapped_window_page_table_builder.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "nec_sg2_vr5500_dram.h"
#include "nec_sg2_vr5500_family_id.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

namespace {

constexpr UnmappedWindowDram kDram = UnmappedWindowPageTableBuilder::CachedWindowDram(
    NecSg2Vr5500Dram::kPaBase, NecSg2Vr5500Dram::kSize, 0u, NecSg2Vr5500Dram::kSize);

class NecSg2Vr5500PageTableBuilder : public UnmappedWindowPageTableBuilder {
public:
    using UnmappedWindowPageTableBuilder::UnmappedWindowPageTableBuilder;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::NecSg2Vr5500;
    }

    std::vector<DramRegion> CachedDramRegions() const override {
        std::vector<DramRegion> r;
        kDram.AddCachedTo(r);
        return r;
    }

    std::vector<BackedRegion> BackedMemoryRegions() const override {
        std::vector<BackedRegion> r;
        kDram.AddBackedTo(r);
        return r;
    }

    std::vector<DramRegion> MappedVaSpans() const override {
        std::vector<DramRegion> r;
        kDram.AddMappedTo(r);
        return r;
    }
};

}  /* namespace */

REGISTER_SERVICE_AS(NecSg2Vr5500PageTableBuilder, PageTableBuilder);

}

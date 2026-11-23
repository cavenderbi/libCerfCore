#include "../unmapped_window_page_table_builder.h"

#include <cerfcore/boards/board_context.h>
#include "../../core/board_config.h"
#include "../dram_card_param_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/memory_kind.h>
#include "ibm_workpad_z50_dram_layout.h"
#include "ibm_workpad_z50_family_id.h"
#include "ibm_workpad_z50_member_straps.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

namespace {

constexpr UnmappedWindowDram kDram = UnmappedWindowPageTableBuilder::CachedWindowDram(
    0x00000000u, kIbmWorkpadZ50DramBankBytes, 0u, kIbmWorkpadZ50DramBankBytes);
constexpr uint32_t kChecksumWordBytes = 4u;

class IbmWorkpadZ50PageTableBuilder : public UnmappedWindowPageTableBuilder {
public:
    using UnmappedWindowPageTableBuilder::UnmappedWindowPageTableBuilder;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
    }

    void OnReady() override {
        const auto& straps = emu_.Get<IbmWorkpadZ50MemberStraps>();
        card_ = CachedWindowRomSpan(CachedWindowVa(straps.RomCardPa()), CachedWindowVa(kIbmWorkpadZ50RomSpaceEndPa));
        dram_ = { kDram };
        const uint32_t card_bytes = emu_.Get<BoardConfig>().SelectedBytes(BoardConfigParamId::DramCard);
        for (uint32_t off = 0; off < card_bytes; off += kIbmWorkpadZ50DramBankBytes) {
            dram_.push_back(CachedWindowDram(kIbmWorkpadZ50ExpansionDramPa + off, kIbmWorkpadZ50DramBankBytes, 0u,
                                             kIbmWorkpadZ50DramBankBytes));
        }
    }

    std::vector<DramRegion> CachedDramRegions() const override {
        std::vector<DramRegion> r;
        for (const UnmappedWindowDram& d : dram_) d.AddCachedTo(r);
        return r;
    }

    std::vector<BackedRegion> BackedMemoryRegions() const override {
        std::vector<BackedRegion> r;
        for (const UnmappedWindowDram& d : dram_) d.AddBackedTo(r);
        r.push_back({ card_.va_base + kChecksumWordBytes, card_.pa_base + kChecksumWordBytes,
                      card_.size - kChecksumWordBytes, MemoryKind::kRom });
        return r;
    }

private:
    std::vector<UnmappedWindowDram> dram_;
    DramRegion                      card_{};
};

}

REGISTER_SERVICE_AS(IbmWorkpadZ50PageTableBuilder, PageTableBuilder);

}

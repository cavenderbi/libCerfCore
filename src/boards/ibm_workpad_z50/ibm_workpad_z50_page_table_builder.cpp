#include "../mips_kseg_page_table_builder.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/memory_kind.h>
#include "ibm_workpad_z50_family_id.h"
#include "ibm_workpad_z50_member_straps.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

namespace {

constexpr DramRegion kDram              = { 0x80000000u, 0x00000000u, 0x01000000u };
constexpr uint32_t   kKseg0Base         = 0x80000000u;
constexpr uint32_t   kChecksumWordBytes = 4u;

class IbmWorkpadZ50PageTableBuilder : public MipsKsegPageTableBuilder {
public:
    using MipsKsegPageTableBuilder::MipsKsegPageTableBuilder;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
    }

    void OnReady() override {
        const auto& straps = emu_.Get<IbmWorkpadZ50MemberStraps>();
        card_ = Kseg0RomSpan(kKseg0Base + straps.RomCardPa(),
                             kKseg0Base + kIbmWorkpadZ50RomSpaceEndPa);
    }

    std::vector<DramRegion> CachedDramRegions() const override { return { kDram }; }

    std::vector<BackedRegion> BackedMemoryRegions() const override {
        return {
            { kDram.va_base, kDram.pa_base, kDram.size, MemoryKind::kRam },
            { card_.va_base + kChecksumWordBytes, card_.pa_base + kChecksumWordBytes,
              card_.size - kChecksumWordBytes, MemoryKind::kRom },
        };
    }

    std::vector<DramRegion> MappedVaSpans() const override { return { kDram, card_ }; }

private:
    DramRegion card_{};
};

}

REGISTER_SERVICE_AS(IbmWorkpadZ50PageTableBuilder, PageTableBuilder);

}

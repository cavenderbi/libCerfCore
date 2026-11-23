#include "casio_cassiopeia_a10_card_windows.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../core/board_parts.h"
#include "casio_cassiopeia_a10_cf_slot.h"
#include "casio_cassiopeia_a10_compact_flash_part_id.h"
#include "casio_cassiopeia_a10_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kAttributeBase = 0x14000000u;

class CasioCassiopeiaA10CfSlotWindows : public CasioCassiopeiaA10CardWindows {
public:
    using CasioCassiopeiaA10CardWindows::CasioCassiopeiaA10CardWindows;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               emu_.Get<BoardParts>().Has(PartId::CasioCassiopeiaA10CompactFlash);
    }

    void OnReady() override {
        cf_slot_ = &emu_.Get<CasioCassiopeiaA10CfSlot>();
        CasioCassiopeiaA10CardWindows::OnReady();
    }

    uint32_t MmioBase() const override { return kAttributeBase; }

protected:
    PcmciaSlot& CardSlot() override { return cf_slot_->Slot(); }

    bool SpaceSelected(Space space) override {
        switch (space) {
        case Space::kAttribute:
            return cf_slot_->AttributeSelected();
        case Space::kCommon:
            return !cf_slot_->AttributeSelected();
        case Space::kIo:
            break;
        }
        return true;
    }

private:
    CasioCassiopeiaA10CfSlot* cf_slot_ = nullptr;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10CfSlotWindows);

}

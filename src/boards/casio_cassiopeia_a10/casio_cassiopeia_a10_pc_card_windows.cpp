#include "casio_cassiopeia_a10_card_windows.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_pc_card.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kAttributeBase = 0x18000000u;

class CasioCassiopeiaA10PcCardWindows : public CasioCassiopeiaA10CardWindows {
public:
    using CasioCassiopeiaA10CardWindows::CasioCassiopeiaA10CardWindows;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    uint32_t MmioBase() const override { return kAttributeBase; }

protected:
    PcmciaSlot& CardSlot() override { return emu_.Get<CasioCassiopeiaA10PcCard>().Slot(); }
};

}

REGISTER_SERVICE(CasioCassiopeiaA10PcCardWindows);

}

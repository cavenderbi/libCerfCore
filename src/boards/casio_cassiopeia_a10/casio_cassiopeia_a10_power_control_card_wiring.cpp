#include "casio_cassiopeia_a10_card_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_chip_intc.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"
#include "casio_cassiopeia_a10_pc_card.h"
#include "casio_cassiopeia_a10_power_control.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint16_t kSlotSupply = 0x0004u;

class CasioCassiopeiaA10PowerControlCardWiring : public CasioCassiopeiaA10CardWiring {
public:
    using CasioCassiopeiaA10CardWiring::CasioCassiopeiaA10CardWiring;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               !CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override {
        power_control_ = &emu_.Get<CasioCassiopeiaA10PowerControl>();
        chip_intc_     = &emu_.Get<CasioCassiopeiaA10ChipIntc>();
        power_control_->AddChangeListener([this] { emu_.Get<CasioCassiopeiaA10PcCard>().ApplyPower(); });
    }

    bool SupplyOn() override { return (power_control_->Value() & kSlotSupply) != 0u; }

    void Drive(const Lines& lines) override {
        chip_intc_->SetInputLevel(CasioCassiopeiaA10ChipInput::kPcCardEvents, lines.change || lines.ireq_latch);
    }

private:
    CasioCassiopeiaA10PowerControl* power_control_ = nullptr;
    CasioCassiopeiaA10ChipIntc*     chip_intc_     = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10PowerControlCardWiring, CasioCassiopeiaA10CardWiring);

}

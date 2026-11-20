#include "casio_cassiopeia_a10_card_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"
#include "casio_cassiopeia_a10_pc_card.h"
#include "casio_cassiopeia_a10_pin_registers.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10SocketVccCardWiring : public CasioCassiopeiaA10CardWiring {
public:
    using CasioCassiopeiaA10CardWiring::CasioCassiopeiaA10CardWiring;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { pins_ = &emu_.Get<CasioCassiopeiaA10PinRegisters>(); }

    bool SupplyOn() override { return emu_.Get<CasioCassiopeiaA10PcCard>().SocketVccOn(); }

    void Drive(const Lines& lines) override {
        pins_->SetLine(kCasioCassiopeiaA10Pin04C, !lines.card_present);
        pins_->SetLine(kCasioCassiopeiaA10Pin04E, !lines.ireq_low);
    }

private:
    CasioCassiopeiaA10PinRegisters* pins_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10SocketVccCardWiring, CasioCassiopeiaA10CardWiring);

}

#include "../hitachi_pcc/hitachi_pcc_card_bus.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../boards/hp_600/hp_600_family_id.h"
#include "hd64461_pcc.h"

namespace cerfcore {

namespace {

class Hd64461PccCardBus : public HitachiPccCardBus {
public:
    using HitachiPccCardBus::HitachiPccCardBus;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override { pcc_ = &emu_.Get<Hd64461Pcc>(); }

protected:
    HitachiPcc& Pcc() override { return *pcc_; }

private:
    Hd64461Pcc* pcc_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hd64461PccCardBus, Sh77xxPcmciaCardBus);

}

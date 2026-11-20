#include "../../peripherals/hitachi_pcc/hitachi_pcc_card_bus.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7707_id.h"
#include "sh7707_pcc.h"

namespace cerfcore {

namespace {

class Sh7707PccCardBus : public HitachiPccCardBus {
public:
    using HitachiPccCardBus::HitachiPccCardBus;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }

    void OnReady() override { pcc_ = &emu_.Get<Sh7707Pcc>(); }

protected:
    HitachiPcc& Pcc() override { return *pcc_; }

private:
    Sh7707Pcc* pcc_ = nullptr;
};

}

REGISTER_SERVICE_AS(Sh7707PccCardBus, Sh77xxPcmciaCardBus);

}

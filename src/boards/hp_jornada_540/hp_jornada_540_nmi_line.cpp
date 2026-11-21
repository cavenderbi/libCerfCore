#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/service.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

class HpJornada540NmiLine : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

    void OnReady() override { emu_.Get<Sh77xxIntc>().SetNmiLevel(true); }
};

}

REGISTER_SERVICE(HpJornada540NmiLine);

}

#include "../sh77xx_nmi_power_fail_input.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

class Hp300PowerFailInput : public Sh77xxNmiPowerFailInput {
public:
    using Sh77xxNmiPowerFailInput::Sh77xxNmiPowerFailInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }
};

}

REGISTER_SERVICE(Hp300PowerFailInput);

}

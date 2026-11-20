#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/touch/host_pen_target_touch_input.h"
#include "hp_300_family_id.h"
#include "hp_300_touch_panel.h"

namespace cerfcore {

namespace {

class Hp300TouchInput : public HostPenTargetTouchInput {
public:
    using HostPenTargetTouchInput::HostPenTargetTouchInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

protected:
    HostPenTarget& Target() override { return emu_.Get<Hp300TouchPanel>(); }
};

}

REGISTER_SERVICE_AS(Hp300TouchInput, TouchInput);

}

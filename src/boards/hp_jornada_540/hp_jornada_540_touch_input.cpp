#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/touch/host_pen_target_touch_input.h"
#include "hp_jornada_540_family_id.h"
#include "hp_jornada_540_touch_panel.h"

namespace cerfcore {

namespace {

class HpJornada540TouchInput : public HostPenTargetTouchInput {
public:
    using HostPenTargetTouchInput::HostPenTargetTouchInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

protected:
    HostPenTarget& Target() override { return emu_.Get<HpJornada540TouchPanel>(); }
};

}

REGISTER_SERVICE_AS(HpJornada540TouchInput, TouchInput);

}

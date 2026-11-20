#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/touch/host_pen_target_touch_input.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_touch_adc.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10TouchInput : public HostPenTargetTouchInput {
public:
    using HostPenTargetTouchInput::HostPenTargetTouchInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

protected:
    HostPenTarget& Target() override { return emu_.Get<CasioCassiopeiaA10TouchAdc>(); }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10TouchInput, TouchInput);

}

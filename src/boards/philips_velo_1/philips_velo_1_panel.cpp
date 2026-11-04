#include "../../socs/pr31x00/pr31x00_panel.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "philips_velo_1_family_id.h"

namespace cerfcore {

namespace {

class PhilipsVelo1Panel : public Pr31x00Panel {
public:
    using Pr31x00Panel::Pr31x00Panel;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::PhilipsVelo1;
    }

    bool Color() const override { return false; }
};

}

REGISTER_SERVICE_AS(PhilipsVelo1Panel, Pr31x00Panel);

}

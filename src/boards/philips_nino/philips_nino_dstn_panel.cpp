#include "../../socs/pr31x00/pr31x00_panel.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "philips_nino_500_id.h"

namespace cerfcore {

namespace {

class PhilipsNinoDstnPanel : public Pr31x00Panel {
public:
    using Pr31x00Panel::Pr31x00Panel;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardId() == BoardId::PhilipsNino500;
    }

    bool Color() const override { return true; }
};

}

REGISTER_SERVICE_AS(PhilipsNinoDstnPanel, Pr31x00Panel);

}

#include "../../socs/pr31x00/pr31x00_panel.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "philips_nino_200_id.h"
#include "philips_nino_300_id.h"
#include "philips_nino_312_id.h"

namespace cerfcore {

namespace {

class PhilipsNinoMstnPanel : public Pr31x00Panel {
public:
    using Pr31x00Panel::Pr31x00Panel;

    bool ShouldRegister() override {
        const std::string_view id = emu_.Get<BoardContext>().GetBoardId();
        return id == BoardId::PhilipsNino200 || id == BoardId::PhilipsNino300 ||
               id == BoardId::PhilipsNino312;
    }

    bool Color() const override { return false; }
};

}

REGISTER_SERVICE_AS(PhilipsNinoMstnPanel, Pr31x00Panel);

}

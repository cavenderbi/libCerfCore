#include "../../socs/pr31x00/pr31x00_panel.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "sharp_mobilon_hc4000_id.h"
#include "sharp_mobilon_hc4100_id.h"
#include "sharp_mobilon_hc4100_8mb_id.h"

namespace cerfcore {

namespace {

class SharpMobilonHcMstnPanel : public Pr31x00Panel {
public:
    using Pr31x00Panel::Pr31x00Panel;

    bool ShouldRegister() override {
        const std::string_view id = emu_.Get<BoardContext>().GetBoardId();
        return id == BoardId::SharpMobilonHc4000 || id == BoardId::SharpMobilonHc4100 ||
               id == BoardId::SharpMobilonHc41008mb;
    }

    bool Color() const override { return false; }
};

}

REGISTER_SERVICE_AS(SharpMobilonHcMstnPanel, Pr31x00Panel);

}

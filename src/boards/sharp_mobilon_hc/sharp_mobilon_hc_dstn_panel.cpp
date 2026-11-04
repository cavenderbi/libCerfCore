#include "../../socs/pr31x00/pr31x00_panel.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "sharp_mobilon_hc4500_id.h"
#include "sharp_mobilon_hc4500_8mb_id.h"
#include "sharp_mobilon_hc4600_id.h"

namespace cerfcore {

namespace {

class SharpMobilonHcDstnPanel : public Pr31x00Panel {
public:
    using Pr31x00Panel::Pr31x00Panel;

    bool ShouldRegister() override {
        const std::string_view id = emu_.Get<BoardContext>().GetBoardId();
        return id == BoardId::SharpMobilonHc4500 || id == BoardId::SharpMobilonHc45008mb ||
               id == BoardId::SharpMobilonHc4600;
    }

    bool Color() const override { return true; }
};

}

REGISTER_SERVICE_AS(SharpMobilonHcDstnPanel, Pr31x00Panel);

}

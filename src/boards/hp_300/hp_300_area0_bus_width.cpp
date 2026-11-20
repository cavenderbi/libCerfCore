#include "../../socs/sh77xx/sh77xx_area0_bus_width.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

class Hp300Area0BusWidth : public Sh77xxArea0BusWidth {
public:
    using Sh77xxArea0BusWidth::Sh77xxArea0BusWidth;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    uint32_t Bytes() const override { return 2u; }
};

}

REGISTER_SERVICE_AS(Hp300Area0BusWidth, Sh77xxArea0BusWidth);

}

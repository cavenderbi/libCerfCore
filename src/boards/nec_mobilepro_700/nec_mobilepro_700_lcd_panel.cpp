#include "../nec_vr41xx_lcd_panel.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_mobilepro_700_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

class NecMobilepro700LcdPanel : public NecVr41xxLcdPanel {
public:
    using NecVr41xxLcdPanel::NecVr41xxLcdPanel;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro700;
    }

    uint32_t Width() const override { return 640u; }
    uint32_t Height() const override { return 240u; }
    uint32_t StrideBytes() const override { return 256u; }
    uint32_t VramBytes() const override { return 0x00028000u; }
    int      DisplayEnableGpio() const override { return 11; }
};

}

REGISTER_SERVICE_AS(NecMobilepro700LcdPanel, NecVr41xxLcdPanel);

}

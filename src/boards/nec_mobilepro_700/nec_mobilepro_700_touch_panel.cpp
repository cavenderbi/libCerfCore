#include "../nec_vr41xx_touch_panel.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../nec_vrc4172_battery.h"
#include "nec_mobilepro_700_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr NecBatteryAdcStep kMainPack[] = {
    {859u, 100}, {818u, 90}, {795u, 80}, {774u, 70}, {747u, 60},
    {727u, 50},  {709u, 40}, {691u, 30}, {665u, 20}, {0u, 10},
};

constexpr uint16_t kBackupCellHealthy = 859u;

class NecMobilepro700TouchPanel : public NecVr41xxTouchPanel {
public:
    using NecVr41xxTouchPanel::NecVr41xxTouchPanel;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro700;
    }

protected:
    uint16_t MainBatteryWord() const override { return emu_.Get<NecVrc4172Battery>().AdcWord(kMainPack); }
    uint16_t BackupCellWord() const override { return kBackupCellHealthy; }
};

}

REGISTER_SERVICE_AS(NecMobilepro700TouchPanel, Vr41xxPiuPanel);

}

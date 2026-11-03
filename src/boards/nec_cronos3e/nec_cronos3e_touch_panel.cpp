#include "../nec_vr41xx_touch_panel.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../nec_vrc4172_battery.h"
#include "nec_cronos3e_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr NecBatteryAdcStep kOnBatteryPack[] = {
    {821u, 100}, {797u, 80}, {777u, 60}, {767u, 40}, {758u, 20}, {0u, 10},
};

constexpr NecBatteryAdcStep kOnAcPack[] = {
    {823u, 90}, {791u, 60}, {0u, 20},
};

constexpr uint16_t kBackupCellHealthy = 847u;

class NecCronos3eTouchPanel : public NecVr41xxTouchPanel {
public:
    using NecVr41xxTouchPanel::NecVr41xxTouchPanel;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecCronos3e;
    }

protected:
    uint16_t MainBatteryWord() const override {
        const NecVrc4172Battery& battery = emu_.Get<NecVrc4172Battery>();
        return battery.IsOnBattery() ? battery.AdcWord(kOnBatteryPack) : battery.AdcWord(kOnAcPack);
    }
    uint16_t BackupCellWord() const override { return kBackupCellHealthy; }
};

}

REGISTER_SERVICE_AS(NecCronos3eTouchPanel, Vr41xxPiuPanel);

}

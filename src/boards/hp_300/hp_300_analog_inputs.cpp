#include "../../socs/sh77xx/sh77xx_analog_inputs.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "hp_300_battery.h"
#include "hp_300_family_id.h"
#include "hp_300_touch_panel.h"

namespace cerfcore {

namespace {

constexpr uint32_t kTouchXChannel      = 0u;
constexpr uint32_t kTouchYChannel      = 1u;
constexpr uint32_t kMainBatteryChannel = 2u;
constexpr uint32_t kBackupCellChannel  = 3u;
constexpr uint16_t kBackupCellAdc      = 0x384u;

class Hp300AnalogInputs : public Sh77xxAnalogInputs {
public:
    using Sh77xxAnalogInputs::Sh77xxAnalogInputs;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override {
        battery_ = &emu_.Get<Hp300Battery>();
        touch_   = &emu_.Get<Hp300TouchPanel>();
    }

    uint16_t Convert(uint32_t channel) override {
        if (channel == kTouchXChannel) return touch_->ConvertPlate(Hp300TouchPanel::Plate::kX);
        if (channel == kTouchYChannel) return touch_->ConvertPlate(Hp300TouchPanel::Plate::kY);
        if (channel == kMainBatteryChannel) return battery_->MainBatteryAdc();
        if (channel == kBackupCellChannel) return kBackupCellAdc;
        emu_.Get<Fatal>().Die("Hp300AnalogInputs: A/D conversion of AN%u", channel);
    }

private:
    Hp300Battery*    battery_ = nullptr;
    Hp300TouchPanel* touch_   = nullptr;
};

}

REGISTER_SERVICE_AS(Hp300AnalogInputs, Sh77xxAnalogInputs);

}

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_indicators.h>
#include <cerfcore/host/machine_object_tree.h>
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "hp_600_family_id.h"
#include "hp_600_battery.h"
#include "hp_600_input_nets.h"
#include "hp_600_led_colors.h"
#include "hp_600_notification_led_net.h"

#include <cstdint>
#include <optional>
#include <string>

namespace cerfcore {

namespace {

constexpr uint32_t  kFlashHalfMs = 500u;
constexpr ObjectRgb kClrOrange{255, 150, 40};
constexpr ObjectRgb kClrRed{235, 50, 40};
constexpr ObjectRgb kClrGreen{120, 255, 140};

enum class Charge { kDark, kCharging, kCharged };

ObjectRgb LedRgb(Hp600LedColor color) {
    switch (color) {
        case Hp600LedColor::kRed:   return kClrRed;
        case Hp600LedColor::kGreen: return kClrGreen;
        default:                    return kClrOrange;
    }
}

class Hp600Led : public Service, public MachineLamp {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        wiring_  = &emu_.Get<Sh77xxPortWiring>();
        battery_ = &emu_.Get<Hp600Battery>();
        battery_->AddChangeListener([this] { Refresh(); });
        emu_.Get<Hp600InputNets>().AddNetListener(Hp600InputNet::kNotificationLedLoop, [this] { Refresh(); });
        const Hp600LedColors& colors = emu_.Get<Hp600LedColors>();
        charging_     = colors.ChargingColor();
        notification_ = colors.NotificationColor();
        emu_.Get<MachineObjectTree>().Add(this);
    }

    std::string ObjectId() const override { return "led"; }
    static constexpr char kObjectName[] = "LED";
    std::string ObjectName() const override { return kObjectName; }

    LampState Lamp() const override {
        const Charge    charge = CurrentCharge();
        const ObjectRgb charge_color =
            charge == Charge::kCharged ? LedRgb(Hp600LedColor::kGreen) : LedRgb(charging_);
        if (!Flashing()) return LampState{charge != Charge::kDark, false, charge_color};
        std::optional<ObjectRgb> between;
        if (charge != Charge::kDark) between = charge_color;
        return LampState{true, true, LedRgb(notification_), kFlashHalfMs, kFlashHalfMs, between};
    }

private:
    Charge CurrentCharge() const {
        if (battery_->Charging()) return Charge::kCharging;
        return battery_->OnExternalPower() ? Charge::kCharged : Charge::kDark;
    }

    bool Flashing() const { return (wiring_->NetLevels(kSh77xxPortJ) & kHp600PortJNotificationLed) == 0u; }

    Sh77xxPortWiring* wiring_       = nullptr;
    Hp600Battery*     battery_      = nullptr;
    Hp600LedColor     charging_     = Hp600LedColor::kOrange;
    Hp600LedColor     notification_ = Hp600LedColor::kOrange;
};

}

REGISTER_SERVICE(Hp600Led);

}

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfcore/host/machine_indicators.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/state/hibernation.h>
#include <cerfengine/host_request_channel.h>
#include "../../execution/engine_bridge.h"
#include "../../socs/guest_sample_clock.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "hp_600_family_id.h"
#include "hp_600_battery.h"
#include "hp_600_input_nets.h"
#include "../sh77xx_port_key_matrix.h"
#include "hp_600_keyboard_layout.h"
#include "hp_600_led_colors.h"
#include "hp_600_notification_led_net.h"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace cerfcore {

namespace {

constexpr uint8_t kVkLedButton = 0x78u;
constexpr GuestCycleClock::Rate kPressHold{10u, 1u};
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

class Hp600LedButton : public Service, public MachineLamp {
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
        led_cell_     = emu_.Get<Hp600KeyboardLayout>().ScannedCellOf(kVkLedButton);
        if (led_cell_) {
            matrix_   = &emu_.Get<Sh77xxPortKeyMatrix>();
            requests_ = &emu_.Get<EngineBridge>().Engine().Get<cerfengine::HostRequestChannel>();
            requests_->RegisterListener([this] { ApplyPress(); });
            hold_.Attach([this] { Release(); });
            emu_.Get<GuestDeepSleep>().RegisterParkClock([this] { ApplyPress(); });
            emu_.Get<Hibernation>().RegisterPostRestoreListener([this] {
                press_pending_.store(false, std::memory_order_release);
                hold_.Stop();
            });
        }
        auto& tree = emu_.Get<MachineObjectTree>();
        tree.Add(this);
        if (led_cell_) tree.Add(&button_, this);
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
    class Button final : public MachineButton {
    public:
        Button(Hp600LedButton& owner, CerfEmulator& emu) : MachineButton(emu), owner_(owner) {}
        std::string ObjectId() const override { return "button"; }
        static constexpr char kObjectName[] = "LED button";
        std::string ObjectName() const override { return kObjectName; }

    private:
        bool Pressed() override {
            owner_.Press();
            return true;
        }

        Hp600LedButton& owner_;
    };

    Charge CurrentCharge() const {
        if (battery_->Charging()) return Charge::kCharging;
        return battery_->OnExternalPower() ? Charge::kCharged : Charge::kDark;
    }

    bool Flashing() const { return (wiring_->NetLevels(kSh77xxPortJ) & kHp600PortJNotificationLed) == 0u; }

    void Press() {
        press_pending_.store(true, std::memory_order_release);
        requests_->Request();
    }

    void ApplyPress() {
        if (!press_pending_.exchange(false, std::memory_order_acq_rel)) return;
        matrix_->SetKey(*led_cell_, true);
        hold_.Start(kPressHold);
    }

    void Release() {
        hold_.Stop();
        matrix_->SetKey(*led_cell_, false);
    }

    Sh77xxPortWiring*                wiring_   = nullptr;
    Hp600Battery*                    battery_  = nullptr;
    Sh77xxPortKeyMatrix*             matrix_   = nullptr;
    cerfengine::HostRequestChannel*  requests_ = nullptr;
    std::optional<uint32_t>          led_cell_;
    Hp600LedColor                    charging_     = Hp600LedColor::kOrange;
    Hp600LedColor                    notification_ = Hp600LedColor::kOrange;
    GuestSampleClock                 hold_{emu_};
    std::atomic<bool>                press_pending_{false};
    Button                           button_{*this, emu_};
};

}

REGISTER_SERVICE(Hp600LedButton);

}

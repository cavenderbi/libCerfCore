#include "hp_600_led.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/state/state_stream.h>
#include "hp_600_battery.h"
#include "hp_600_family_id.h"
#include "hp_600_input_nets.h"
#include "hp_600_keyboard_layout.h"
#include "../sh77xx_port_key_matrix.h"

#include <cstdint>
#include <optional>

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

}

REGISTER_SERVICE(Hp600Led);

bool Hp600Led::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
}

void Hp600Led::OnReady() {
    battery_ = &emu_.Get<Hp600Battery>();
    inputs_  = &emu_.Get<Hp600InputNets>();
    matrix_  = &emu_.Get<Sh77xxPortKeyMatrix>();
    battery_->AddChangeListener([this] { Refresh(); });
    const Hp600LedColors& colors = emu_.Get<Hp600LedColors>();
    charging_     = colors.ChargingColor();
    notification_ = colors.NotificationColor();
    cell_ = emu_.Get<Hp600KeyboardLayout>().LedButtonCell();
    emu_.Get<MachineObjectTree>().Add(this);
}

void Hp600Led::TrackDrive(bool notification_drive_high) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        changed = drive_high_ != notification_drive_high;
        if (changed && !notification_drive_high) dismissed_ = false;
        drive_high_ = notification_drive_high;
        DriveLoopLocked();
    }
    if (changed) Refresh();
}

void Hp600Led::PressButton(bool pressed) {
    if (cell_) matrix_->SetKey(*cell_, pressed);
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const bool released = button_down_ && !pressed;
        button_down_        = pressed;
        changed             = released && !dismissed_;
        if (changed) dismissed_ = true;
        if (changed) DriveLoopLocked();
        LOG(Periph, "[HP600LED] button %s, PJ1 %s, latch %s\n", pressed ? "down" : "up", drive_high_ ? "high" : "low",
            dismissed_ ? "cleared" : "armed");
    }
    if (changed) Refresh();
}

void Hp600Led::DriveLoopLocked() { inputs_->Drive(Hp600InputNet::kNotificationLedLoop, drive_high_ || dismissed_); }

void Hp600Led::SaveObjectState(StateWriter& w) const {
    std::lock_guard<std::mutex> lk(mtx_);
    w.Write<uint8_t>("notification_drive_high", drive_high_ ? 1u : 0u);
    w.Write<uint8_t>("notification_dismissed", dismissed_ ? 1u : 0u);
}

void Hp600Led::RestoreObjectState(StateReader& r) {
    uint8_t drive_high = 0u;
    uint8_t dismissed  = 0u;
    r.Read("notification_drive_high", drive_high);
    r.Read("notification_dismissed", dismissed);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        drive_high_  = drive_high != 0u;
        dismissed_   = dismissed != 0u;
        button_down_ = false;
        DriveLoopLocked();
    }
    Refresh();
}

LampState Hp600Led::Lamp() const {
    bool flashing = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        flashing = !drive_high_ && !dismissed_;
    }
    Charge charge = Charge::kDark;
    if (battery_->Charging()) charge = Charge::kCharging;
    else if (battery_->OnExternalPower()) charge = Charge::kCharged;
    const ObjectRgb charge_color = charge == Charge::kCharged ? LedRgb(Hp600LedColor::kGreen) : LedRgb(charging_);
    if (!flashing) return LampState{charge != Charge::kDark, false, charge_color};
    std::optional<ObjectRgb> between;
    if (charge != Charge::kDark) between = charge_color;
    return LampState{true, true, LedRgb(notification_), kFlashHalfMs, kFlashHalfMs, between};
}

}

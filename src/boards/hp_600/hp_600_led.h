#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_indicators.h>
#include "hp_600_led_colors.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace cerfcore {

class Hp600Battery;
class Hp600InputNets;
class Sh77xxPortKeyMatrix;

class Hp600Led : public Service, public MachineLamp {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void TrackDrive(bool notification_drive_high);
    void PressButton(bool pressed);

    std::string ObjectId() const override { return "led"; }
    static constexpr char kObjectName[] = "LED";
    std::string ObjectName() const override { return kObjectName; }
    LampState   Lamp() const override;

    void SaveObjectState(StateWriter& w) const override;
    void RestoreObjectState(StateReader& r) override;

private:
    void DriveLoopLocked();

    Hp600Battery*           battery_      = nullptr;
    Hp600InputNets*         inputs_       = nullptr;
    Sh77xxPortKeyMatrix*    matrix_       = nullptr;
    std::optional<uint32_t> cell_;
    Hp600LedColor           charging_     = Hp600LedColor::kOrange;
    Hp600LedColor           notification_ = Hp600LedColor::kOrange;
    mutable std::mutex      mtx_;
    bool                    drive_high_  = false;
    bool                    dismissed_   = false;
    bool                    button_down_ = false;
};

}

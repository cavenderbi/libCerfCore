#pragma once

#include "machine_object.h"

#include "../core/callback_list.h"

#include <functional>
#include <mutex>
#include <string>

namespace cerfcore {

class CerfEmulator;

class Battery : public MachineObject {
public:
    explicit Battery(CerfEmulator& emu) : emu_(emu) {}

    std::string       ObjectId() const override { return "battery"; }
    static constexpr char kObjectName[] = "Battery";
    std::string       ObjectName() const override { return kObjectName; }
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kBattery; }

    static constexpr int kFullPercent = 100;

    static bool ChargingAt(bool on_external_power, int percent) {
        return on_external_power && percent < kFullPercent;
    }

    int  ChargePercent() const;
    bool OnBattery() const;
    bool OnExternalPower() const;
    bool Charging() const;
    void SetChargePercent(int percent);
    void SetOnBattery(bool on_battery);

    void AddChangeListener(std::function<void()> cb) { on_change_.Add(std::move(cb)); }

    void SaveObjectState(StateWriter& w) const override;
    void RestoreObjectState(StateReader& r) override;

private:
    void UpdateFromHost(const std::function<void()>& write);
    void Propagate();

    CerfEmulator&         emu_;
    mutable std::mutex    mtx_;
    CallbackList<>        on_change_;
    bool                  on_battery_     = false;
    int                   charge_percent_ = kFullPercent;
};

}

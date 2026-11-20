#include <cerfcore/host/battery.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/emulation_freeze.h>
#include <cerfcore/state/state_stream.h>

#include <algorithm>
#include <cstdint>

namespace cerfcore {

int Battery::ChargePercent() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return charge_percent_;
}

bool Battery::OnBattery() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return on_battery_;
}

bool Battery::OnExternalPower() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return !on_battery_;
}

bool Battery::Charging() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return ChargingAt(!on_battery_, charge_percent_);
}

void Battery::SetChargePercent(int percent) {
    UpdateFromHost([&] { charge_percent_ = std::clamp(percent, 0, kFullPercent); });
}

void Battery::SetOnBattery(bool on_battery) {
    UpdateFromHost([&] { on_battery_ = on_battery; });
}

void Battery::UpdateFromHost(const std::function<void()>& write) {
    auto frozen = emu_.Get<EmulationFreeze>().WorkerSection();
    {
        std::lock_guard<std::mutex> lk(mtx_);
        write();
    }
    Propagate();
}

void Battery::Propagate() {
    on_change_.Call();
    Changed();
}

void Battery::SaveObjectState(StateWriter& w) const {
    std::lock_guard<std::mutex> lk(mtx_);
    w.Write<uint8_t>("on_battery", on_battery_ ? 1u : 0u);
    w.Write<int32_t>("fill_percent", charge_percent_);
}

void Battery::RestoreObjectState(StateReader& r) {
    uint8_t on      = 0;
    int32_t percent = kFullPercent;
    r.Read("on_battery", on);
    r.Read("fill_percent", percent);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        on_battery_     = on != 0u;
        charge_percent_ = percent;
    }
    Propagate();
}

}

#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

#include <cstdint>
#include <span>

namespace cerfcore {

class Sh77xxIntc;

class Hp300Battery : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    bool     OnExternalPower() const;
    uint16_t MainBatteryAdc() const;
    void     SetChargerEnabled(bool enabled);

private:
    void DriveExternalPowerInterruptPin();

    Sh77xxIntc*               intc_ = nullptr;
    std::span<const uint16_t> level_adc_;
    bool                      charger_enabled_ = false;
    Battery                   battery_{emu_};
};

}

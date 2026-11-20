#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

namespace cerfcore {

class SharpMobilonHcBattery : public Service {
public:
    explicit SharpMobilonHcBattery(CerfEmulator& e) : Service(e) {}
    bool ShouldRegister() override;
    void OnReady() override;

    int  FillPercent() const { return battery_.ChargePercent(); }
    bool IsOnBattery() const { return battery_.OnBattery(); }
    bool OnExternalPower() const { return battery_.OnExternalPower(); }

private:
    Battery       battery_{emu_};
};

}

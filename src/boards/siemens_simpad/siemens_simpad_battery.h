#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

namespace cerfcore {

class SiemensSimpadBattery : public Service {
public:
    explicit SiemensSimpadBattery(CerfEmulator& e) : Service(e) {}
    bool ShouldRegister() override;
    void OnReady() override;

    int  FillPercent() const { return battery_.ChargePercent(); }
    bool IsOnBattery() const { return battery_.OnBattery(); }
    bool Charging() const { return battery_.Charging(); }

private:
    Battery       battery_{emu_};
};

}

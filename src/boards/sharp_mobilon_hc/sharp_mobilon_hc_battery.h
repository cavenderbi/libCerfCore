#pragma once

#include <cerfcore/core/service.h>
#include "../../host/battery_widget.h"

namespace cerfcore {

class SharpMobilonHcBattery : public Service {
public:
    explicit SharpMobilonHcBattery(CerfEmulator& e) : Service(e) {}
    bool ShouldRegister() override;
    void OnReady() override;

    int  FillPercent() const { return battery_.FillPercent(); }  /* 0..100 */
    bool IsOnBattery() const { return battery_.IsOnBattery(); }

private:
    BatteryWidget battery_;
};

}

#pragma once

#include <cerfcore/core/service.h>
#include "../../peripherals/i2c/i2c_bit_bang_target.h"

#include <optional>

namespace cerfcore {

class IbmWorkpadZ50BatteryGauge;
class StateReader;
class StateWriter;
class Vr41xxGiu;

class IbmWorkpadZ50BatterySmbus : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

private:
    void OnPins();
    void DriveSda(bool released);

    Vr41xxGiu*                      giu_   = nullptr;
    IbmWorkpadZ50BatteryGauge*      gauge_ = nullptr;
    std::optional<I2cBitBangTarget> target_;
    bool                            sda_released_ = true;
};

}

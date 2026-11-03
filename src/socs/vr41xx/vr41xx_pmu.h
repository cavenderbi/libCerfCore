#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

namespace cerfcore {

class Vr41xxPmu : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual void OnGpioLevel(int pin, bool prev, bool level) = 0;
    virtual void LatchRtcAlarmWake() = 0;
    virtual void SetPowerSwitch(bool high) = 0;
    virtual bool PowerHoldsWakeFactors() const = 0;
};

}

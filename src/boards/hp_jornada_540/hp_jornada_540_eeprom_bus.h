#pragma once

#include <cerfcore/core/service.h>
#include "../../peripherals/i2c/i2c_bit_bang_target.h"

#include <optional>

namespace cerfcore {

class HpJornada540Eeprom;
class StateReader;
class StateWriter;

class HpJornada540EepromBus : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void OnLines(bool scl, bool sda);
    bool SdaReleased() const { return sda_released_; }

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

private:
    HpJornada540Eeprom*             eeprom_ = nullptr;
    std::optional<I2cBitBangTarget> target_;
    bool                            sda_released_ = true;
};

}

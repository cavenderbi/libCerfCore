#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>
#include "nec_battery_adc_steps.h"

#include <cstddef>
#include <cstdint>

namespace cerfcore {

class NecVrc4172Battery : public Service {
public:
    using Service::Service;

    void OnReady() override;

    int  FillPercent() const { return battery_.ChargePercent(); }
    bool IsOnBattery() const { return battery_.OnBattery(); }

    template <size_t N>
    uint16_t AdcWord(const NecBatteryAdcStep (&table)[N]) const {
        return NecBatteryAdcWord(table, FillPercent());
    }

private:
    void DriveAcPin();

    Battery       battery_{emu_};
};

}

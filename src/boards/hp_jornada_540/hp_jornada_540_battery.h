#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

#include <cstdint>
#include <functional>
#include <utility>

namespace cerfcore {

class HpJornada540Battery : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    bool     OnExternalPower() const { return battery_.OnExternalPower(); }
    bool     Charging() const { return battery_.Charging(); }
    uint16_t MainBatteryCode() const;
    void     AddChangeListener(std::function<void()> cb) { battery_.AddChangeListener(std::move(cb)); }

private:
    Battery battery_{emu_};
};

}

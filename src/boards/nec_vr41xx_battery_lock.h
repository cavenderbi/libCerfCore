#pragma once

#include <cerfcore/core/service.h>

#include <string_view>

namespace cerfcore {

class NecVr41xxBatteryLock : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

protected:
    virtual std::string_view BatteryLockFamilyId() const = 0;
    virtual int              BatteryLockPin() const      = 0;
};

}

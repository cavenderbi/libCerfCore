#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Pdiusbd12Config : public Service {
public:
    using Service::Service;

    virtual uint32_t DataPortPa() const = 0;
    virtual uint32_t CommandPortPa() const = 0;
    virtual void     DriveInterrupt(bool asserted) = 0;
    virtual bool     InterruptWakesStandby() = 0;
};

}

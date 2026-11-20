#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/socs/guest_cpu_reset.h>

#include <cstdint>

namespace cerfcore {

class Intel82365slBoard : public Service {
public:
    using Service::Service;

    virtual uint8_t WiredIrqSelect() const = 0;
    virtual bool    CardInputPullupsSwitched() const = 0;
    virtual bool    HostSamplesMemcs16() const = 0;
    virtual bool    ResetDriveAtReset(ResetLineKind kind) = 0;
    virtual void    SetIrqLevel(bool level) = 0;
};

}

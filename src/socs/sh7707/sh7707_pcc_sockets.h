#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_object.h>

#include <cstdint>

namespace cerfcore {

class Sh7707PccSockets : public Service {
public:
    using Service::Service;

    virtual bool           Fitted(uint32_t channel) const = 0;
    virtual ObjectIdentity Socket(uint32_t channel) const = 0;
};

}

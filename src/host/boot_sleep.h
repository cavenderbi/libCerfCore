#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/guest_power_notifier.h>

namespace cerfcore {

class BootSleep : public Service {
public:
    using Service::Service;

    virtual ResumeSource Waker() const = 0;
};

}

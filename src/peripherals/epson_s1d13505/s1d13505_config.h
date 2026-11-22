#pragma once

#include <cerfcore/core/service.h>
#include "../../execution/guest_cycle_clock.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

class S1d13505Config : public Service {
public:
    using Service::Service;

    virtual uint32_t              HostWindowBase() const = 0;
    virtual GuestCycleClock::Rate ClkiRate() const = 0;
    virtual void                  RegisterClkiListener(std::function<void()> fn) = 0;
    virtual uint16_t              ConfigurationPins() const = 0;
    virtual uint8_t               GpioInputLevels() const = 0;
};

}

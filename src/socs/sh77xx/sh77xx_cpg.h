#pragma once

#include <cerfcore/core/service.h>
#include "../../execution/guest_cycle_clock.h"

#include <cstdint>

namespace cerfcore {

class Sh77xxCpg : public Service {
public:
    using Service::Service;

    virtual GuestCycleClock::Rate ResetCpuRate() const = 0;
    virtual GuestCycleClock::Rate CpuRate(uint16_t frqcr) const = 0;
    virtual GuestCycleClock::Rate PeripheralRate(uint16_t frqcr) const = 0;

    uint64_t AttachToCkioPin();
    bool     CkioPinAttached() const { return ckio_pin_attached_; }

    GuestCycleClock::Rate InternalCkioRate() const;

protected:
    uint64_t BoardCkioHz() const;

private:
    bool ckio_pin_attached_ = false;
};

}

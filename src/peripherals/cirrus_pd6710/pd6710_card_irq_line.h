#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Pd6710CardIrqLine : public Service {
public:
    using Service::Service;
    ~Pd6710CardIrqLine() override = default;

    virtual uint8_t WiredIrqSelect() const = 0;
    virtual bool    IrqPinDrivesNothing(uint8_t select) const = 0;
    virtual void Assert()   = 0;
    virtual void Deassert() = 0;
};

}

#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class RicohRf5c396Board : public Service {
public:
    using Service::Service;

    virtual uint32_t IsaIoBase()  const = 0;
    virtual uint32_t IsaMemBase() const = 0;
    virtual uint8_t  WiredIrqSelect() const = 0;
    virtual bool     GpiWiredToVs1(int bank) const = 0;
    virtual void     SetIrqLevel(bool level) = 0;
};

}

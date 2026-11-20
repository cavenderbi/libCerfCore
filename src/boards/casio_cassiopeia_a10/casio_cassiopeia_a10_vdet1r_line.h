#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class CasioCassiopeiaA10Vdet1rLine : public Service {
public:
    using Service::Service;

    virtual uint16_t StatusBits() const = 0;
    virtual uint8_t  PortPins() const   = 0;
    virtual uint8_t  PortLevels() const = 0;
};

}

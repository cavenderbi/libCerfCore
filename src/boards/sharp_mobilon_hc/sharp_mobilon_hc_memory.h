#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class SharpMobilonHcMemory : public Service {
public:
    using Service::Service;

    virtual uint32_t MemConfig0Banks() const = 0;
    virtual uint32_t Bank0Bytes() const = 0;
    virtual uint32_t Bank1Bytes() const = 0;
    virtual bool     Cs1RamTag() const = 0;
};

}

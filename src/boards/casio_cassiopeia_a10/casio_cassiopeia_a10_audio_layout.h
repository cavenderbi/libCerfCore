#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class CasioCassiopeiaA10AudioLayout : public Service {
public:
    using Service::Service;

    virtual uint32_t UnitShift() const = 0;
    virtual uint32_t SlotBytes() const = 0;
};

}

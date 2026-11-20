#pragma once

#include <cerfcore/core/service.h>
#include "sh77xx_dmac.h"

#include <cstdint>

namespace cerfcore {

class Sh77xxDmaAddressMap : public Service {
public:
    using Service::Service;

    uint32_t BusAddress(uint32_t channel, uint32_t addr, uint32_t bytes, const char* side) const;
    bool     NamesDataRegister(Sh77xxDmaRequest source, uint32_t sar, uint32_t dar) const;

protected:
    virtual bool ReachesChipModule(uint32_t pa, uint32_t bytes) const = 0;
};

}

#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class CasioJx670DmaBlock : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    uint32_t RequireBlock(uint32_t start_va, uint32_t end_va, uint32_t rate_hz, uint16_t channels,
                          bool capture) const;

private:
    uint32_t GuestBlockBytes(uint32_t rate_hz, uint16_t channels, bool capture) const;
};

}

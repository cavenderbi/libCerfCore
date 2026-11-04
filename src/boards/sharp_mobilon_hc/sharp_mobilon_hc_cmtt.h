#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class SharpMobilonHcCmtt : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    /* CS3+0x00, 32-bit. */
    void     WriteDataPort(uint32_t value);
    uint32_t ReadDataPort();

    uint8_t ReadStatusByte();

    bool ReadReady() const;

private:
    bool write_ready_ = false;
};

}

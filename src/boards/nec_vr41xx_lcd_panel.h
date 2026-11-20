#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class NecVr41xxLcdPanel : public Service {
public:
    using Service::Service;

    virtual uint32_t Width() const       = 0;
    virtual uint32_t Height() const      = 0;
    virtual uint32_t StrideBytes() const = 0;
    virtual uint32_t VramBytes() const   = 0;
    virtual int      DisplayEnableGpio() const = 0;
};

}

#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Sh77xxArea0BusWidth : public Service {
public:
    using Service::Service;

    virtual uint32_t Bytes() const = 0;
};

}

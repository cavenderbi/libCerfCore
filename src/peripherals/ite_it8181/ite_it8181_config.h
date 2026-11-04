#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class IteIt8181Config : public Service {
public:
    using Service::Service;

    virtual uint32_t HostWindowBase() const = 0;

    virtual uint8_t Revision() const = 0;
};

}

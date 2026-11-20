#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Hd6417096ClockSeed : public Service {
public:
    using Service::Service;

    virtual uint64_t PowerOnSeconds() = 0;
};

}

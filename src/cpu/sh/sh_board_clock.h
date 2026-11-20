#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Sh77xxCpg;

class ShBoardClock : public Service {
public:
    using Service::Service;

    virtual bool Pll1Switchable() const = 0;

private:
    friend class Sh77xxCpg;

    virtual uint64_t CkioHz() const = 0;
};

}

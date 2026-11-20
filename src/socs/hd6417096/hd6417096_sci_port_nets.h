#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class Hd6417096SciPortNets : public Service {
public:
    using Service::Service;

    virtual bool Bit6High() const = 0;
};

}

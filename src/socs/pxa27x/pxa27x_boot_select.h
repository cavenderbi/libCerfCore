#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class Pxa27xBootSelect : public Service {
public:
    using Service::Service;

    virtual bool BootsFrom16BitMemory() const = 0;
};

}

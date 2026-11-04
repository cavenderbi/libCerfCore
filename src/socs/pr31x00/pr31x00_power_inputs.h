#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class Pr31x00PowerInputs : public Service {
public:
    using Service::Service;

    virtual bool PwrIntAsserted() const = 0;
};

}

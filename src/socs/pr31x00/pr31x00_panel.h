#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class Pr31x00Panel : public Service {
public:
    using Service::Service;

    virtual bool Color() const = 0;
};

}

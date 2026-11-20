#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class CasioCassiopeiaA10Speaker : public Service {
public:
    using Service::Service;

    virtual bool On() const = 0;
};

}

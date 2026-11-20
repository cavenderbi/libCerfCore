#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class CasioCassiopeiaA10AudioStatusClear : public Service {
public:
    using Service::Service;

    virtual bool ClearsOnRead() const = 0;
};

}

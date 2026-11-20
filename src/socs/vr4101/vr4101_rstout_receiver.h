#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class Vr4101RstoutReceiver : public Service {
public:
    using Service::Service;

    virtual void RstoutChanged(bool high) = 0;
};

}

#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/service.h>

namespace cerfcore {

class CasioCassiopeiaA10RtsLine : public Service {
public:
    using Service::Service;

    virtual bool Asserted() const                                     = 0;
    virtual void AddChangeListener(CallbackList<>::Callback listener) = 0;
};

}

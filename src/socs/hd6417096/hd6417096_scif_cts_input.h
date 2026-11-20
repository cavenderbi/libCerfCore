#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/service.h>

namespace cerfcore {

class Hd6417096ScifCtsInput : public Service {
public:
    using Service::Service;

    virtual bool High() const                                         = 0;
    virtual void AddChangeListener(CallbackList<>::Callback listener) = 0;
};

}

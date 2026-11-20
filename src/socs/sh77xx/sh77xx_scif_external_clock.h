#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

struct Sh77xxScifChannel;

class Sh77xxScifExternalClock : public Service {
public:
    using Service::Service;

    virtual uint32_t ClockHz(const Sh77xxScifChannel& channel) const = 0;
    virtual void     AddChangeListener(CallbackList<>::Callback listener) = 0;
};

}

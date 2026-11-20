#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

enum class CasioCassiopeiaA10LedMode : uint8_t {
    kOff,
    kBlinking,
    kSteady,
};

class CasioCassiopeiaA10LedEnable : public Service {
public:
    using Service::Service;

    virtual CasioCassiopeiaA10LedMode Mode(uint16_t on_time) const                   = 0;
    virtual void                      AddChangeListener(CallbackList<>::Callback listener) = 0;
};

}

#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/service.h>
#include "../../execution/guest_cycle_clock.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

class HpJornada540LcdClock : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void                  SetDivider(uint32_t divider);
    GuestCycleClock::Rate Rate() const { return {ckio_hz_, divider_}; }
    void                  RegisterListener(std::function<void()> fn) { listeners_.Add(std::move(fn)); }

private:
    uint64_t       ckio_hz_ = 0;
    uint32_t       divider_ = 1;
    CallbackList<> listeners_;
};

}

#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>
#include <functional>
#include <utility>

namespace cerfcore {

inline constexpr uint32_t kSh77xxBcr2FirstArea = 2u;
inline constexpr uint32_t kSh77xxBcr2LastArea  = 6u;

class Sh77xxBsc : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual bool     HasSdramModeRegister() const      = 0;
    virtual bool     PortEnabled() const               = 0;
    virtual bool     AreaIsPcmcia(uint32_t area) const = 0;
    virtual uint32_t AreaBusBytes(uint32_t area) const = 0;

    void AddBusWidthListener(std::function<void()> listener) { bus_width_listeners_.Add(std::move(listener)); }

protected:
    void NotifyBusWidthChanged() { bus_width_listeners_.Call(); }

private:
    CallbackList<> bus_width_listeners_;
};

}

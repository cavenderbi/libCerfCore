#pragma once

#include "../socs/sh77xx/sh77xx_serial_wiring.h"

#include <cerfcore/core/callback_list.h>

#include <atomic>
#include <functional>

namespace cerfcore {

class Sh77xxSerialLinesWiring : public Sh77xxSerialWiring {
public:
    using Sh77xxSerialWiring::Sh77xxSerialWiring;

    bool               DriverPowered(uint32_t channel_base) const override;
    SerialControlLines Control(uint32_t channel_base) const override;
    void SetControlListener(uint32_t channel_base, std::function<void()> listener) override;

protected:
    static constexpr uint32_t kNoChannel = 0u;

    void SetLines(uint32_t connected_base, bool powered, bool dtr, bool rts, uint32_t joined_base = kNoChannel);

private:
    std::atomic<uint32_t> connected_{kNoChannel};
    std::atomic<uint32_t> joined_{kNoChannel};
    std::atomic<bool>     powered_{false};
    std::atomic<bool>     dtr_{false};
    std::atomic<bool>     rts_{false};
    CallbackList<>        listeners_;
};

}

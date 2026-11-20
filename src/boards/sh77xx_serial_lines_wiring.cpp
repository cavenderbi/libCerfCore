#include "sh77xx_serial_lines_wiring.h"

#include <utility>

namespace cerfcore {

bool Sh77xxSerialLinesWiring::DriverPowered(uint32_t channel_base) const {
    const bool linked = channel_base == connected_.load(std::memory_order_acquire) ||
                        channel_base == joined_.load(std::memory_order_acquire);
    return channel_base != kNoChannel && linked && powered_.load(std::memory_order_acquire);
}

SerialControlLines Sh77xxSerialLinesWiring::Control(uint32_t) const {
    SerialControlLines lines;
    lines.dtr = dtr_.load(std::memory_order_acquire);
    lines.rts = rts_.load(std::memory_order_acquire);
    return lines;
}

void Sh77xxSerialLinesWiring::SetControlListener(uint32_t, std::function<void()> listener) {
    listeners_.Add(std::move(listener));
}

void Sh77xxSerialLinesWiring::SetLines(uint32_t connected_base, bool powered, bool dtr, bool rts,
                                       uint32_t joined_base) {
    bool changed = connected_.exchange(connected_base, std::memory_order_acq_rel) != connected_base;
    if (joined_.exchange(joined_base, std::memory_order_acq_rel) != joined_base) changed = true;
    if (powered_.exchange(powered, std::memory_order_acq_rel) != powered) changed = true;
    if (dtr_.exchange(dtr, std::memory_order_acq_rel) != dtr) changed = true;
    if (rts_.exchange(rts, std::memory_order_acq_rel) != rts) changed = true;
    if (changed) listeners_.Call();
}

}

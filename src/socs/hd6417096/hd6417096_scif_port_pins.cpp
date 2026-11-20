#include "hd6417096_scif_port_pins.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hd6417096_id.h"

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(Hd6417096ScifPortPins);

namespace {

constexpr uint8_t kRts2PortIo = 0x80u;
constexpr uint8_t kRts2PortDt = 0x40u;
constexpr uint8_t kTxd2PortIo = 0x20u;
constexpr uint8_t kTxd2PortDt = 0x10u;

}

bool Hd6417096ScifPortPins::ShouldRegister() { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }

void Hd6417096ScifPortPins::Store(uint8_t bits) {
    if (bits_.exchange(static_cast<uint8_t>(bits & kHd6417096ScifPortBits), std::memory_order_acq_rel) !=
        (bits & kHd6417096ScifPortBits)) {
        listeners_.Call();
    }
}

void Hd6417096ScifPortPins::Load(uint8_t bits) {
    bits_.store(static_cast<uint8_t>(bits & kHd6417096ScifPortBits), std::memory_order_release);
}

uint8_t Hd6417096ScifPortPins::Bits() const { return bits_.load(std::memory_order_acquire); }

bool Hd6417096ScifPortPins::TxdPortLow() const {
    const uint8_t bits = Bits();
    return (bits & kTxd2PortIo) != 0u && (bits & kTxd2PortDt) == 0u;
}

void Hd6417096ScifPortPins::SetRtsFunctionHigh(bool high) {
    if (function_high_.exchange(high, std::memory_order_acq_rel) != high) listeners_.Call();
}

bool Hd6417096ScifPortPins::RtsHigh() const {
    const uint8_t bits = Bits();
    if ((bits & kRts2PortIo) == 0u) return function_high_.load(std::memory_order_acquire);
    return (bits & kRts2PortDt) != 0u;
}

void Hd6417096ScifPortPins::AddChangeListener(CallbackList<>::Callback listener) { listeners_.Add(std::move(listener)); }

}

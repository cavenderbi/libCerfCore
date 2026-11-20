#include "../sh77xx/sh77xx_sci_port.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "hd6417096_id.h"
#include "hd6417096_sci_port_nets.h"
#include "hd6417096_scif_port_pins.h"

#include <atomic>
#include <cstdint>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kScsptr      = 0x1FFFFF7Cu;
constexpr uint8_t  kRts2PortIo  = 0x80u;
constexpr uint8_t  kRts2PortDt  = 0x40u;
constexpr uint8_t  kSpb1Bits    = 0x0Cu;
constexpr uint8_t  kSpb0Bits    = 0x03u;
constexpr uint8_t  kSpb0io      = 0x02u;
constexpr uint8_t  kSpb0dt      = 0x01u;
constexpr uint8_t  kSpb2dt      = 0x10u;
constexpr uint8_t  kRxdPinsIdle = kSpb2dt | kSpb0dt;

class Hd6417096SciPort : public Sh77xxSciPort {
public:
    using Sh77xxSciPort::Sh77xxSciPort;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }

    void OnReady() override {
        nets_      = emu_.TryGet<Hd6417096SciPortNets>();
        scif_pins_ = &emu_.Get<Hd6417096ScifPortPins>();
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
            if (!emu_.Get<GuestCpuReset>().PowerOnReset(kind)) return;
            spb0_.store(0u, std::memory_order_release);
            scif_pins_->Load(0u);
        });
    }

    uint32_t MmioBase() const override { return kScsptr; }
    uint32_t MmioSize() const override { return 1u; }

    uint8_t ReadByte(uint32_t) override {
        const bool    cts2_high = nets_ != nullptr && nets_->Bit6High();
        const uint8_t latch     = static_cast<uint8_t>(Latch() & ~kRts2PortDt);
        return static_cast<uint8_t>(latch | kRxdPinsIdle | (cts2_high ? kRts2PortDt : 0u));
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        if ((value & kSpb1Bits) != 0u || ((value & kRts2PortIo) != 0u && nets_ == nullptr)) {
            HaltUnsupportedAccess("WriteByte", addr, value);
        }
        const uint8_t spb0 = static_cast<uint8_t>(value & kSpb0Bits);
        const bool    sci  = spb0_.exchange(spb0, std::memory_order_acq_rel) != spb0;
        scif_pins_->Store(value);
        if (sci) change_listeners_.Call();
    }

    void SaveState(StateWriter& w) override { w.Write("latch", Latch()); }

    void RestoreState(StateReader& r) override {
        uint8_t value = 0u;
        r.Read("latch", value);
        spb0_.store(static_cast<uint8_t>(value & kSpb0Bits), std::memory_order_release);
        scif_pins_->Load(value);
    }

    void SetTransmitter(bool enabled) override {
        if (transmitter_.exchange(enabled, std::memory_order_acq_rel) != enabled) change_listeners_.Call();
    }

    bool TxdPinLow() const override {
        const uint8_t spb0 = spb0_.load(std::memory_order_acquire);
        return !transmitter_.load(std::memory_order_acquire) && (spb0 & kSpb0io) != 0u && (spb0 & kSpb0dt) == 0u;
    }

    void AddChangeListener(CallbackList<>::Callback listener) override { change_listeners_.Add(std::move(listener)); }

private:
    uint8_t Latch() const {
        return static_cast<uint8_t>(spb0_.load(std::memory_order_acquire) | scif_pins_->Bits());
    }

    Hd6417096SciPortNets*  nets_      = nullptr;
    Hd6417096ScifPortPins* scif_pins_ = nullptr;
    std::atomic<uint8_t>   spb0_{0u};
    std::atomic<bool>      transmitter_{false};
    CallbackList<>         change_listeners_;
};

}

REGISTER_SERVICE_AS(Hd6417096SciPort, Sh77xxSciPort);

}

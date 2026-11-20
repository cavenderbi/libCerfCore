#include "../sh77xx/sh77xx_port_data.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../sh77xx/sh77xx_bsc.h"
#include "../sh77xx/sh77xx_port_wiring.h"
#include "hd6417096_id.h"
#include "hd6417096_port.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kPctr      = 0x1FFFFF76u;
constexpr uint32_t kPdtr      = 0x1FFFFF78u;
constexpr uint32_t kPort      = 0u;
constexpr uint16_t kPinOutput = 0x0001u;
constexpr uint16_t kPinNoPull = 0x0002u;

class Hd6417096PortData : public Sh77xxPortData {
public:
    using Sh77xxPortData::Sh77xxPortData;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }

    void OnReady() override {
        wiring_ = &emu_.Get<Sh77xxPortWiring>();
        bsc_    = &emu_.Get<Sh77xxBsc>();
        bsc_->AddBusWidthListener([this] { TrackDrive(); });
        emu_.Get<PeripheralDispatcher>().Register(this);
        GuestCpuReset& reset = emu_.Get<GuestCpuReset>();
        reset.RegisterResetListener([this](ResetLineKind kind) {
            if (emu_.Get<GuestCpuReset>().PowerOnReset(kind)) pctr_ = 0u;
        });
        reset.RegisterResetReleaseListener([this] { TrackDrive(); });
        TrackDrive();
    }

    uint32_t MmioBase() const override { return kPctr; }
    uint32_t MmioSize() const override { return kPdtr + 1u - kPctr; }

    PinDrive Drive(uint32_t, uint32_t pin) const override {
        if (!bsc_->PortEnabled()) return PinDrive::kFunction;
        if (PinIsOutput(pin)) return ((latch_ >> pin) & 1u) != 0u ? PinDrive::kHigh : PinDrive::kLow;
        return (PinControl(pin) & kPinNoPull) != 0u ? PinDrive::kUndriven : PinDrive::kPulledUp;
    }

    uint16_t ReadHalf(uint32_t addr) override {
        if (addr != kPctr || !bsc_->PortEnabled()) HaltUnsupportedAccess("ReadHalf", addr, 0u);
        return pctr_;
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        if (addr != kPctr || !bsc_->PortEnabled()) HaltUnsupportedAccess("WriteHalf", addr, value);
        pctr_ = value;
        TrackDrive();
    }

    uint8_t ReadByte(uint32_t addr) override {
        if (addr != kPdtr || !bsc_->PortEnabled()) HaltUnsupportedAccess("ReadByte", addr, 0u);
        const uint8_t outputs = OutputPins();
        const uint8_t wired   = wiring_->WiredInputs(kPort);
        for (uint32_t pin = 0; pin < kHd6417096PortPins; ++pin) {
            const uint8_t bit = static_cast<uint8_t>(1u << pin);
            if ((outputs & bit) == 0u && (wired & bit) == 0u) {
                emu_.Get<Fatal>().Die("Hd6417096PortData: read of port pin %u as an input", pin);
            }
        }
        return static_cast<uint8_t>((latch_ & outputs) | (wiring_->NetLevels(kPort) & ~outputs));
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        if (addr != kPdtr || !bsc_->PortEnabled()) HaltUnsupportedAccess("WriteByte", addr, value);
        latch_ = value;
        TrackDrive();
    }

    void SaveState(StateWriter& w) override {
        w.Write("pctr", pctr_);
        w.Write("latch", latch_);
        wiring_->SaveState(w);
    }

    void RestoreState(StateReader& r) override {
        r.Read("pctr", pctr_);
        r.Read("latch", latch_);
        wiring_->RestoreState(r);
    }

    void PostRestore() override { TrackDrive(); }

private:
    uint16_t PinControl(uint32_t pin) const { return static_cast<uint16_t>(pctr_ >> (2u * pin)); }

    bool PinIsOutput(uint32_t pin) const { return (PinControl(pin) & kPinOutput) != 0u; }

    uint8_t OutputPins() const {
        uint8_t pins = 0u;
        for (uint32_t pin = 0; pin < kHd6417096PortPins; ++pin) {
            if (PinIsOutput(pin)) pins = static_cast<uint8_t>(pins | (1u << pin));
        }
        return pins;
    }

    void TrackDrive() {
        wiring_->TrackDrive(*this);
        NotifyDriveChanged();
    }

    Sh77xxPortWiring* wiring_ = nullptr;
    Sh77xxBsc*        bsc_    = nullptr;
    uint16_t          pctr_   = 0u;
    uint8_t           latch_  = 0u;
};

}

REGISTER_SERVICE_AS(Hd6417096PortData, Sh77xxPortData);

}

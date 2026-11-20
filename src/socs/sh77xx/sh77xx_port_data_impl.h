#pragma once

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "sh77xx_intc.h"
#include "sh77xx_pfc.h"
#include "sh77xx_port_data.h"
#include "sh77xx_port_wiring.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

namespace sh77xx_port_data {

inline constexpr const char* kPortNames[kSh77xxPortCount] = {"A", "B", "C", "D", "E", "F",
                                                             "G", "H", "J", "K", "L", "SC"};

struct PortData {
    uint8_t       input_only;
    uint8_t       undefined_other_mode;
    uint8_t       reset;
    uint8_t       function_output;
    uint8_t       separate_input = 0u;
    uint8_t       intc_pins      = 0u;
    Sh77xxIntcPin intc_first     = Sh77xxIntcPin::kCount;
    uint8_t       no_pull_up          = 0u;
    uint8_t       other_is_input      = 0u;
    uint8_t       intc_modes          = 0u;
    uint8_t       irl_modes           = 0u;
    uint8_t       function_intc_pins  = 0u;
    Sh77xxIntcPin function_intc_first = Sh77xxIntcPin::kCount;
    uint8_t       receiver_input      = 0u;
};

inline constexpr uint8_t kIntcInOtherMode          = 0x01u;
inline constexpr uint8_t kIntcInInputModes         = 0x0Cu;
inline constexpr uint8_t kIntcInPortModes          = 0x0Eu;
inline constexpr uint8_t kIntcInOtherAndInputModes = 0x0Du;

struct Model {
    PortData port[kSh77xxPortCount];
};

constexpr uint32_t LowestPin(uint8_t mask) {
    uint32_t pin = 0u;
    while (pin < kSh77xxPortPins && ((mask >> pin) & 1u) == 0u) ++pin;
    return pin;
}

template <const std::string_view& Soc, const Model& M>
class Sh77xxPortDataBase : public Sh77xxPortData {
public:
    using Sh77xxPortData::Sh77xxPortData;

    bool ShouldRegister() override { return this->emu_.template Get<BoardContext>().GetSocId() == Soc; }

    void OnReady() override {
        pfc_    = &this->emu_.template Get<Sh77xxPfc>();
        intc_   = &this->emu_.template Get<Sh77xxIntc>();
        wiring_ = &this->emu_.template Get<Sh77xxPortWiring>();
        ResetLatches();
        this->emu_.template Get<PeripheralDispatcher>().Register(this);
        GuestCpuReset& reset = this->emu_.template Get<GuestCpuReset>();
        reset.RegisterResetListener([this](ResetLineKind kind) {
            if (this->emu_.template Get<GuestCpuReset>().PowerOnReset(kind)) ResetLatches();
        });
        reset.RegisterResetReleaseListener([this] { TrackDrive(); });
        pfc_->AddControlListener([this] { TrackDrive(); });
        this->AddFunctionListener([this] { TrackDrive(); });
        TrackDrive();
    }

    uint32_t MmioBase() const override { return kSh77xxPortDataBase; }
    uint32_t MmioSize() const override { return kSh77xxPortCount * kSh77xxPortDataStride; }

    Sh77xxIntcPin IntcPinOf(uint32_t port, uint32_t pin) const override { return IntcPin(M.port[port], pin); }

    PinDrive Drive(uint32_t port, uint32_t pin) const override {
        const PortData& p    = M.port[port];
        const uint8_t   bit  = static_cast<uint8_t>(1u << pin);
        const uint32_t  mode = pfc_->PinMode(port, pin);
        if ((this->Transmitting(port) & bit) != 0u) return PinDrive::kFunction;
        if (mode == kSh77xxPinModeOutput) {
            if ((p.input_only & bit) != 0u) return PinDrive::kUndriven;
            return (latch_[port] & bit) != 0u ? PinDrive::kHigh : PinDrive::kLow;
        }
        if ((p.separate_input & bit) != 0u) return PinDrive::kUndriven;
        switch (mode) {
            case kSh77xxPinModeOther:
                if ((this->FunctionOutputsLow(port) & bit) != 0u) return PinDrive::kLow;
                if ((this->FunctionOutputsHigh(port) & bit) != 0u) return PinDrive::kHigh;
                return (p.function_output & bit) != 0u ? PinDrive::kFunction : PinDrive::kUndriven;
            case kSh77xxPinModePullUpInput:
                return (p.no_pull_up & bit) != 0u ? PinDrive::kUndriven : PinDrive::kPulledUp;
            default:                        return PinDrive::kUndriven;
        }
    }

    uint8_t ReadByte(uint32_t addr) override {
        const uint32_t  port           = Port(addr, "ReadByte", 0u);
        const PortData& p              = M.port[port];
        const uint8_t   wired          = wiring_->WiredInputs(port);
        const uint8_t   nets           = wiring_->NetLevels(port);
        const uint8_t   separate_wired = wiring_->WiredSeparateInputs(port);
        const uint8_t   separate_nets  = wiring_->SeparateInputLevels(port);
        const uint8_t   transmitting   = static_cast<uint8_t>(p.receiver_input & this->Transmitting(port));
        uint8_t         v              = 0u;
        for (uint32_t pin = 0; pin < kSh77xxPortPins; ++pin) {
            const uint32_t mode = pfc_->PinMode(port, pin);
            const uint8_t  bit  = static_cast<uint8_t>(1u << pin);
            const bool     receiver_read = mode == kSh77xxPinModeOutput && (transmitting & bit) != 0u;
            if (receiver_read && (this->Receiving(port) & bit) == 0u) {
                this->emu_.template Get<Fatal>().Die(
                    "Sh77xxPortData: read of port %s pin %u in output mode with its transmitter enabled and its "
                    "receiver disabled",
                    kPortNames[port], pin);
            }
            if (mode >= kSh77xxPinModeInputFirst || receiver_read ||
                (mode == kSh77xxPinModeOther && (p.other_is_input & bit) != 0u)) {
                if ((p.separate_input & bit) != 0u) {
                    if ((separate_wired & bit) == 0u) {
                        this->emu_.template Get<Fatal>().Die(
                            "Sh77xxPortData: read of port %s pin %u from its separate input pin", kPortNames[port],
                            pin);
                    }
                    if ((separate_nets & bit) != 0u) v = static_cast<uint8_t>(v | bit);
                    continue;
                }
                if ((wired & bit) == 0u) {
                    this->emu_.template Get<Fatal>().Die("Sh77xxPortData: read of port %s pin %u in input mode %u",
                                                         kPortNames[port], pin, mode);
                }
                const bool high = (p.intc_pins & bit) != 0u ? intc_->PinLevel(IntcPin(p, pin)) : (nets & bit) != 0u;
                if (high) v = static_cast<uint8_t>(v | bit);
                continue;
            }
            if (mode == kSh77xxPinModeOther && (p.undefined_other_mode & bit) != 0u) {
                this->emu_.template Get<Fatal>().Die("Sh77xxPortData: read of port %s pin %u in mode 0",
                                                     kPortNames[port], pin);
            }
            if ((p.input_only & bit) == 0u) v = static_cast<uint8_t>(v | (latch_[port] & bit));
        }
        return v;
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        const uint32_t  port = Port(addr, "WriteByte", value);
        const PortData& p    = M.port[port];
        for (uint32_t pin = 0; pin < kSh77xxPortPins; ++pin) {
            const uint8_t bit = static_cast<uint8_t>(1u << pin);
            if ((p.undefined_other_mode & bit) != 0u && pfc_->PinMode(port, pin) == kSh77xxPinModeOther) {
                this->emu_.template Get<Fatal>().Die("Sh77xxPortData: write 0x%02X to port %s with pin %u in mode 0",
                                                     value, kPortNames[port], pin);
            }
        }
        latch_[port] = static_cast<uint8_t>(value & ~p.input_only);
        TrackDrive();
    }

    void SaveState(StateWriter& w) override {
        w.WriteBytes("latch", latch_, sizeof(latch_));
        wiring_->SaveState(w);
    }

    void RestoreState(StateReader& r) override {
        r.ReadBytes("latch", latch_, sizeof(latch_));
        wiring_->RestoreState(r);
    }

    void PostRestore() override { TrackDrive(); }

protected:
    uint32_t Port(uint32_t addr, const char* op, uint32_t value) {
        const uint32_t off = addr - kSh77xxPortDataBase;
        if ((off % kSh77xxPortDataStride) != 0u) this->HaltUnsupportedAccess(op, addr, value);
        return off / kSh77xxPortDataStride;
    }

    void TrackDrive() {
        wiring_->TrackDrive(*this);
        PublishDrivenIntcPins();
        this->NotifyDriveChanged();
    }

    Sh77xxPfc* pfc_ = nullptr;

private:
    static Sh77xxIntcPin IntcPin(const PortData& p, uint32_t pin) {
        return static_cast<Sh77xxIntcPin>(static_cast<uint32_t>(p.intc_first) + pin - LowestPin(p.intc_pins));
    }

    void PublishDrivenIntcPins() {
        uint32_t driven     = 0u;
        uint32_t high       = 0u;
        uint32_t absent     = 0u;
        uint32_t absent_irl = 0u;
        for (uint32_t port = 0; port < kSh77xxPortCount; ++port) {
            const PortData& p = M.port[port];
            for (uint32_t pin = 0; pin < kSh77xxPortPins; ++pin) {
                const uint8_t  pin_bit = static_cast<uint8_t>(1u << pin);
                const uint32_t mode    = pfc_->PinMode(port, pin);
                if ((p.function_intc_pins & pin_bit) != 0u && mode != kSh77xxPinModeOther) {
                    absent |= 1u << (static_cast<uint32_t>(p.function_intc_first) + pin -
                                     LowestPin(p.function_intc_pins));
                }
                if ((p.intc_pins & pin_bit) == 0u) continue;
                const Sh77xxIntcPin intc_pin = IntcPin(p, pin);
                const uint32_t      bit      = 1u << static_cast<uint32_t>(intc_pin);
                const PinDrive      drive    = Drive(port, pin);
                if (intc_pin <= Sh77xxIntcPin::kIrq3 && ((p.irl_modes >> mode) & 1u) == 0u) absent_irl |= bit;
                if (((p.intc_modes >> mode) & 1u) == 0u) {
                    absent |= bit;
                    continue;
                }
                if (drive != PinDrive::kHigh && drive != PinDrive::kLow) continue;
                driven |= bit;
                if (drive == PinDrive::kHigh) high |= bit;
            }
        }
        intc_->SetPortDrive(driven, high, absent, absent_irl);
    }

    void ResetLatches() {
        for (uint32_t i = 0; i < kSh77xxPortCount; ++i) {
            latch_[i] = static_cast<uint8_t>(M.port[i].reset & ~M.port[i].input_only);
        }
    }

    Sh77xxIntc*       intc_                    = nullptr;
    Sh77xxPortWiring* wiring_                  = nullptr;
    uint8_t           latch_[kSh77xxPortCount] = {};
};

}

}

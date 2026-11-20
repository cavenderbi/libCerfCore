#pragma once

#include "vr41xx_port_giu.h"
#include "vr41xx_giu_input_pins.h"
#include "vr41xx_giu_interrupts.h"
#include "vr41xx_giu_regs.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../execution/engine_bridge.h"
#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfengine/mips/mips_interrupt_channel.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/state/state_stream.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "vr41xx_icu.h"
#include "vr41xx_pmu.h"
#include <cerfcore/core/callback_list.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace cerfcore {

namespace cerf_vr41xx_giu_detail {

struct Vr41xxGiuModel {
    uint32_t base;
    uint32_t size;
    uint16_t podat_l_power_on;           /* GIUPODATL RTCRST column */
    bool     intstat_sets_while_disabled;
    bool     inten_gates_icu_input;
    bool     podat_l_retained_on_reset;  /* GIUPODATL After-reset / Other-resets column */
    bool     podat_h_retained_on_reset;  /* GIUPODATH After-reset / Other-resets column */
    uint64_t hibernate_undriven;
    uint64_t power_down_high;
    uint16_t pull_pins = 0u;
};

template <const std::string_view& Soc, const Vr41xxGiuModel& M, class Iface>
class Vr41xxGiuCore : public Iface {
public:
    using Iface::Iface;
    using Iface::NotifyOutputDriveChanged;

protected:
    using Iface::emu_;
    using Iface::AlternateDrive;
    using Iface::SavePinDevices;
    using Iface::RestorePinDevices;
    using Iface::HaltUnsupportedAccess;

public:

    bool ShouldRegister() override {
        return this->emu_.template Get<BoardContext>().GetSocId() == Soc;
    }

    void OnReady() override {
        pmu_ = &this->emu_.template Get<Vr41xxPmu>();
        this->emu_.template Get<PeripheralDispatcher>().Register(this);
        this->emu_.template Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
            std::lock_guard<std::mutex> lk(mtx_);
            hibernating_ = false;
            ApplyResetLocked(kind);
        });
        this->emu_.template Get<GuestDeepSleep>().RegisterSleepEntryListener([this] {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                hibernating_ = true;
            }
            NotifyOutputDriveChanged();
        });
        if constexpr (M.power_down_high != 0u) {
            channel_ = &this->emu_.template Get<EngineBridge>().Engine().template Get<cerfengine::MipsInterruptChannel>();
            channel_->RegisterSuspendListener([this] {
                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    suspended_ = channel_->Suspended();
                }
                NotifyOutputDriveChanged();
            });
        }
    }

    uint32_t MmioBase() const override { return M.base; }
    uint32_t MmioSize() const override { return M.size; }

    bool GetPinLevel(int pin) override {
        std::lock_guard<std::mutex> lk(mtx_);
        return ((EffectivePinsLocked() >> pin) & 1u) != 0u;
    }

    uint16_t ReadHalf(uint32_t addr) override {
        std::lock_guard<std::mutex> lk(mtx_);
        return ReadHalfLocked(addr - M.base);
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t off = addr - M.base;
        uint16_t previous = 0;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            previous = podat_l_;
            WriteHalfLocked(off, value);
        }
        if (off == kOffPoDatL) NotifyOutputDataLow(previous);
        if (SetsOutputDrive(off)) NotifyOutputDriveChanged();
    }

    PinDrive OutputDrive(int gpio) override {
        if (const auto drive = AlternateDrive(gpio)) return *drive;
        std::lock_guard<std::mutex> lk(mtx_);
        const uint64_t pin = uint64_t{1} << gpio;
        if ((hibernating_ || suspended_) && (M.power_down_high & pin) != 0u)
            return PinDrive::kHigh;
        if (hibernating_ && (M.hibernate_undriven & pin) != 0u) return PinDrive::kUndriven;
        if (gpio >= 48) {
            const int n = gpio - 48;
            if (((podat_h_ >> (8 + n)) & 1u) == 0u) return PinDrive::kUndriven;
            return Level((podat_h_ >> n) & 1u);
        }
        if (gpio >= 32) return Level((podat_l_ >> (gpio - 32)) & 1u);
        const uint32_t sel = (static_cast<uint32_t>(iosel_h_) << 16) | iosel_l_;
        if (((sel >> gpio) & 1u) == 0u) return PinDrive::kUndriven;
        const uint32_t out = (static_cast<uint32_t>(piod_out_h_) << 16) | piod_out_l_;
        return Level((out >> gpio) & 1u);
    }

    uint32_t ReadWord(uint32_t addr) override {
        std::lock_guard<std::mutex> lk(mtx_);
        const uint32_t off = addr - M.base;
        return static_cast<uint32_t>(ReadHalfLocked(off)) |
               (static_cast<uint32_t>(ReadHalfLocked(off + 2)) << 16);
    }

    void WriteWord(uint32_t addr, uint32_t value) override {
        const uint32_t off = addr - M.base;
        uint16_t previous = 0;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            previous = podat_l_;
            WriteHalfLocked(off,     static_cast<uint16_t>(value & 0xFFFFu));
            WriteHalfLocked(off + 2, static_cast<uint16_t>(value >> 16));
        }
        if (off == kOffPoDatL || off + 2 == kOffPoDatL) NotifyOutputDataLow(previous);
        if (SetsOutputDrive(off) || SetsOutputDrive(off + 2)) NotifyOutputDriveChanged();
    }

    void SetPinLevel(int pin, bool level) override {
        std::lock_guard<std::mutex> lk(mtx_);
        ApplyPinLevelLocked(pin, inputs_.Drive(pin, level));
    }

    void ReleasePin(int pin) override {
        std::lock_guard<std::mutex> lk(mtx_);
        if (const auto edge = inputs_.Release(pin)) ApplyPinLevelLocked(pin, *edge);
    }

    void SaveState(StateWriter& w) override {
        SaveRegisters(w);
        SavePinDevices(w);
    }

    void RestoreState(StateReader& r) override {
        RestoreRegisters(r);
        RestorePinDevices(r);
    }

    void PostRestore() override {
        std::lock_guard<std::mutex> lk(mtx_);
        if constexpr (M.power_down_high != 0u) suspended_ = channel_->Suspended();
        DriveIcuLocked();
    }

protected:
    uint16_t PortDataLow() {
        std::lock_guard<std::mutex> lk(mtx_);
        return podat_l_;
    }

    void AddPortDataLowListener(std::function<void(uint16_t previous)> fn) {
        podat_l_listeners_.Add(std::move(fn));
    }

    uint16_t ReadPull(uint32_t off) {
        std::lock_guard<std::mutex> lk(mtx_);
        switch (off) {
            case kOffUseUpdn:  return inputs_.UseUpdn();
            case kOffTermUpdn: return inputs_.TermUpdn();
            default: this->emu_.template Get<Fatal>().Die("Vr41xxGiu: pull register read at window offset 0x%02X", off);
        }
    }

    void WritePull(uint32_t off, uint16_t value) {
        std::lock_guard<std::mutex> lk(mtx_);
        if ((value & ~M.pull_pins) != 0u)
            this->emu_.template Get<Fatal>().Die("Vr41xxGiu: pull register 0x%02X write 0x%04X sets reserved bits", off, value);
        const uint32_t before = inputs_.Levels();
        switch (off) {
            case kOffUseUpdn:  inputs_.SetUseUpdn(value); break;
            case kOffTermUpdn: inputs_.SetTermUpdn(value); break;
            default: this->emu_.template Get<Fatal>().Die("Vr41xxGiu: pull register write 0x%04X at window offset 0x%02X", value, off);
        }
        CheckPullsInputOnlyLocked();
        const uint32_t changed = before ^ inputs_.Levels();
        for (int pin = 0; pin < 16; ++pin) {
            if (((changed >> pin) & 1u) == 0u) continue;
            const bool prev = ((before >> pin) & 1u) != 0u;
            ApplyPinLevelLocked(pin, Vr41xxGiuPinEdge{prev, !prev});
        }
        EvaluateLevelsLocked();
    }

private:
    void ApplyPinLevelLocked(int pin, Vr41xxGiuPinEdge edge) {
        pmu_->OnGpioLevel(pin, edge.prev, edge.level);
        irq_.OnPinLevel(pin, edge.prev, edge.level);
        DriveIcuLocked();
    }

    void EvaluateLevelsLocked() {
        irq_.EvaluateLevels(inputs_.Established(), inputs_.Levels());
        DriveIcuLocked();
    }

    void CheckPullsInputOnlyLocked() {
        if (inputs_.UseUpdn() != 0u && iosel_l_ != 0u)
            this->emu_.template Get<Fatal>().Die("Vr41xxGiu: GIUUSEUPDN 0x%04X with GIUIOSELL 0x%04X", inputs_.UseUpdn(), iosel_l_);
    }

    void SaveRegisters(StateWriter& w) {
        std::lock_guard<std::mutex> lk(mtx_);
        w.Write("iosel_l", iosel_l_);    w.Write("iosel_h", iosel_h_);
        w.Write("piod_out_l", piod_out_l_); w.Write("piod_out_h", piod_out_h_);
        w.Write("podat_l", podat_l_);
        w.Write("podat_h", podat_h_);
        irq_.Save(w);
        inputs_.Save(w);
        w.Write<uint8_t>("hibernating", hibernating_ ? 1u : 0u);
    }

    void RestoreRegisters(StateReader& r) {
        std::lock_guard<std::mutex> lk(mtx_);
        r.Read("iosel_l", iosel_l_);    r.Read("iosel_h", iosel_h_);
        r.Read("piod_out_l", piod_out_l_); r.Read("piod_out_h", piod_out_h_);
        r.Read("podat_l", podat_l_);
        r.Read("podat_h", podat_h_);
        irq_.Restore(r);
        inputs_.Restore(r);
        uint8_t hibernating = 0u;
        r.Read("hibernating", hibernating);
        hibernating_ = hibernating != 0u;
    }

    /* IOS = 0 reads the pin (VR4111 UM 19.2.3 p401 / 19.2.4 p402, VR4102 UM 18.2.4 p366,
       VR4121 UM 19.2.4 p450); IOS = 1 reads the driven data (casio_cassiopeia_e55 nk.exe
       @0x9E816BB8 lhu / ori 0x200 / sh on 0xAB000106). */
    uint32_t EffectivePinsLocked() const {
        const uint32_t out = (static_cast<uint32_t>(piod_out_h_) << 16) | piod_out_l_;
        const uint32_t sel = (static_cast<uint32_t>(iosel_h_) << 16) | iosel_l_;
        return inputs_.Merged(out, sel);
    }

    uint16_t ReadHalfLocked(uint32_t off) {
        switch (off) {
            case kOffIoSelL: return iosel_l_;
            case kOffIoSelH: return iosel_h_;
            case kOffPiodL:     return static_cast<uint16_t>(EffectivePinsLocked());
            case kOffPiodH:     return static_cast<uint16_t>(EffectivePinsLocked() >> 16);
            case kOffPoDatL:    return podat_l_;
            case kOffPoDatH:    return podat_h_;
            default: {
                uint16_t value = 0u;
                if (!irq_.ReadRegister(off, &value)) HaltUnsupportedAccess("VR41xx GIU ReadHalf", M.base + off, 0);
                return value;
            }
        }
    }

    void WriteHalfLocked(uint32_t off, uint16_t value) {
        switch (off) {
            case kOffIoSelL:
                iosel_l_ = static_cast<uint16_t>(value & kIoSelLWritable);
                CheckPullsInputOnlyLocked();
                return;
            /* GIUIOSELH IOS(31:16) are all R/W (VR4121 UM 19.2.2, VR4102 UM 18.2.2). */
            case kOffIoSelH: iosel_h_ = value; return;

            /* "When the value of the corresponding IOS bit in the GIUIOSEL register is
               '0', writing a value to the PIOD bit does not affect the GPIO pin (the write
               data is ignored)" (VR4121 UM 19.2.3/19.2.4, VR4102 UM 18.2.3/18.2.4). */
            case kOffPiodL:
                piod_out_l_ = static_cast<uint16_t>((piod_out_l_ & ~iosel_l_) |
                                                    (value & iosel_l_));
                return;
            case kOffPiodH:
                piod_out_h_ = static_cast<uint16_t>((piod_out_h_ & ~iosel_h_) |
                                                    (value & iosel_h_));
                return;

            case kOffPoDatL: podat_l_ = value; return;
            case kOffPoDatH: podat_h_ = static_cast<uint16_t>(value & kPoDatHWritable); return;

            default:
                if (!irq_.WriteRegister(off, value)) HaltUnsupportedAccess("VR41xx GIU WriteHalf", M.base + off, value);
                EvaluateLevelsLocked();
                return;
        }
    }

    /* Every GIU register's After-reset row is 0 except GIUPODATL's and GIUPODATH's: on the
       VR4121 both read "Previous value is retained" (UM 19.2.1-19.2.16), while on the
       VR4102 GIUPODATL is all-1 and GIUPODATH is 0 (UM 18.2.1-18.2.16). GIUPIOD's input
       bits track the pin, which no reset drives. */
    void ApplyResetLocked(ResetLineKind kind) {
        iosel_l_    = 0; iosel_h_    = 0;
        piod_out_l_ = 0; piod_out_h_ = 0;
        irq_.Reset();
        if (kind == ResetLineKind::Rtc || !M.podat_l_retained_on_reset) {
            podat_l_ = M.podat_l_power_on;
        }
        if (kind == ResetLineKind::Rtc || !M.podat_h_retained_on_reset) {
            podat_h_ = 0;
        }
        if (kind == ResetLineKind::Rtc) inputs_.ResetPulls();
        EvaluateLevelsLocked();
    }

    void DriveIcuLocked() {
        auto& icu = this->emu_.template Get<Vr41xxIcu>();
        icu.SetGiuLow(irq_.IcuLow(M.inten_gates_icu_input));
        icu.SetGiuHigh(irq_.IcuHigh(M.inten_gates_icu_input));
    }

    void NotifyOutputDataLow(uint16_t previous) {
        podat_l_listeners_.Call(previous);
    }

    static PinDrive Level(uint32_t bit) {
        return bit != 0u ? PinDrive::kHigh : PinDrive::kLow;
    }

    static bool SetsOutputDrive(uint32_t off) {
        return off == kOffIoSelL || off == kOffIoSelH || off == kOffPiodL || off == kOffPiodH ||
               off == kOffPoDatL || off == kOffPoDatH;
    }

    mutable std::mutex mtx_;
    Vr41xxPmu*         pmu_ = nullptr;
    cerfengine::MipsInterruptChannel* channel_ = nullptr;
    bool               hibernating_ = false;
    bool               suspended_   = false;
    CallbackList<uint16_t>                     podat_l_listeners_;

    /* Every GIU register's RTCRST column is 0 except GIUPODATL's (VR4121 UM
       19.2.1-19.2.16, VR4102 UM 18.2.1-18.2.16). */
    uint16_t iosel_l_    = 0, iosel_h_    = 0;
    uint16_t piod_out_l_ = 0, piod_out_h_ = 0;
    uint16_t podat_l_    = M.podat_l_power_on;
    uint16_t podat_h_    = 0;
    Vr41xxGiuInterrupts irq_{M.intstat_sets_while_disabled, false};
    Vr41xxGiuInputPins  inputs_{M.pull_pins};
};

template <const std::string_view& Soc, const Vr41xxGiuModel& M>
class Vr41xxGiuBase : public Vr41xxGiuCore<Soc, M, Vr41xxPortGiu> {
    using Core = Vr41xxGiuCore<Soc, M, Vr41xxPortGiu>;

public:
    using Core::Core;

    uint16_t OutputDataLow() override { return this->PortDataLow(); }

    void RegisterOutputDataLowListener(std::function<void(uint16_t previous)> fn) override {
        this->AddPortDataLowListener(std::move(fn));
    }

    uint16_t ReadPullRegister(uint32_t off) override { return this->ReadPull(off); }

    void WritePullRegister(uint32_t off, uint16_t value) override { this->WritePull(off, value); }
};

}  /* namespace cerf_vr41xx_giu_detail */

}

#pragma once

#include "vr41xx_giu.h"
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
};

template <const std::string_view& Soc, const Vr41xxGiuModel& M>
class Vr41xxGiuBase : public Vr41xxGiu {
public:
    using Vr41xxGiu::Vr41xxGiu;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == Soc;
    }

    void OnReady() override {
        pmu_ = &emu_.Get<Vr41xxPmu>();
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
            std::lock_guard<std::mutex> lk(mtx_);
            hibernating_ = false;
            ApplyResetLocked(kind);
        });
        emu_.Get<GuestDeepSleep>().RegisterSleepEntryListener([this] {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                hibernating_ = true;
            }
            NotifyOutputDriveChanged();
        });
        if constexpr (M.power_down_high != 0u) {
            channel_ = &emu_.Get<EngineBridge>().Engine().template Get<cerfengine::MipsInterruptChannel>();
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
        if (pin < 0 || pin > 31) {
            emu_.Get<Fatal>().Die("Vr41xxGiu::GetPinLevel: pin %d is outside GPIO(31:0)", pin);
        }
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

    uint16_t OutputDataLow() override {
        std::lock_guard<std::mutex> lk(mtx_);
        return podat_l_;
    }

    void RegisterOutputDataLowListener(std::function<void(uint16_t previous)> fn) override {
        podat_l_listeners_.push_back(std::move(fn));
    }

    void RegisterAlternateFunction(int first_gpio, int last_gpio, AlternateFunction fn) override {
        alternates_.push_back({first_gpio, last_gpio, std::move(fn)});
    }

    void RegisterOutputDriveListener(std::function<void()> fn) override {
        drive_listeners_.push_back(std::move(fn));
    }

    void NotifyOutputDriveChanged() override {
        for (auto& fn : drive_listeners_) fn();
    }

    void RegisterPinDeviceState(std::function<void(StateWriter&)> save,
                                std::function<void(StateReader&)> restore) override {
        pin_device_states_.push_back({std::move(save), std::move(restore)});
    }

    Vr41xxPinDrive OutputDrive(int gpio) override {
        for (const auto& a : alternates_) {
            if (gpio < a.first || gpio > a.last) continue;
            if (const auto drive = a.fn(gpio)) return *drive;
        }
        std::lock_guard<std::mutex> lk(mtx_);
        const uint64_t pin = uint64_t{1} << gpio;
        if ((hibernating_ || suspended_) && (M.power_down_high & pin) != 0u)
            return Vr41xxPinDrive::kHigh;
        if (hibernating_ && (M.hibernate_undriven & pin) != 0u) return Vr41xxPinDrive::kUndriven;
        if (gpio >= 48) {
            const int n = gpio - 48;
            if (((podat_h_ >> (8 + n)) & 1u) == 0u) return Vr41xxPinDrive::kUndriven;
            return Level((podat_h_ >> n) & 1u);
        }
        if (gpio >= 32) return Level((podat_l_ >> (gpio - 32)) & 1u);
        const uint32_t sel = (static_cast<uint32_t>(iosel_h_) << 16) | iosel_l_;
        if (((sel >> gpio) & 1u) == 0u) return Vr41xxPinDrive::kUndriven;
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
        if (pin < 0 || pin > 31) {
            emu_.Get<Fatal>().Die("Vr41xxGiu::SetPinLevel: pin %d is outside GPIO(31:0)", pin);
        }
        std::lock_guard<std::mutex> lk(mtx_);

        const bool     high = pin >= 16;
        const uint16_t bit  = static_cast<uint16_t>(1u << (pin & 15));
        uint16_t&      stat = high ? intstat_h_  : intstat_l_;
        const uint16_t typ   = high ? inttyp_h_   : inttyp_l_;
        const uint16_t alsel = high ? intalsel_h_ : intalsel_l_;
        const uint16_t htsel = high ? inthtsel_h_ : inthtsel_l_;
        const uint16_t en    = high ? inten_h_    : inten_l_;

        const uint32_t pinbit = 1u << pin;
        const bool     prev   = (level_ & pinbit) != 0;
        if (level) level_ |= pinbit;
        else       level_ &= ~pinbit;
        pmu_->OnGpioLevel(pin, prev, level);

        /* GIUINTTYP "1: Edge" - "an interrupt is triggered when the signal state changes
           from low to high or from high to low"; "0: Level" - "the level set to the
           corresponding bit in the GIUINTALSEL register is detected", "1: High active /
           0: Low active" (VR4121 UM 19.2.9-19.2.12, VR4102 UM 18.2.9-18.2.12). */
        const bool edge      = (typ & bit) != 0;
        const bool triggered = edge ? (prev != level) : (level == ((alsel & bit) != 0));
        const bool hold      = (htsel & bit) != 0;

        if (triggered) {
            /* INTS is set "when the signal input to the GPIO pin meets the condition set
               via the GIUINTTYPL ... or the GIUINTALSELL register" (VR4121 UM 19.2.5,
               19.2.6), but only "when '1' is set to the corresponding INTE bit in the
               GIUINTENL register and ..." (VR4102 UM 18.2.5, 18.2.6). */
            if constexpr (M.intstat_sets_while_disabled) {
                stat |= bit;
            } else if (en & bit) {
                stat |= bit;
            } else if (hold) {
                /* "If '1' (hold) is set to the INTH bit while the interrupt enable bit is
                   set to 0 (prohibit interrupts), any change in the pin state is retained
                   as change data. Therefore, an interrupt still occurs when the interrupt
                   enable bit is again set to enable interrupts" (VR4102 UM 18.2.13/18.2.14). */
                (high ? retained_h_ : retained_l_) |= bit;
            }
        } else if (!edge && !hold) {
            /* GIUINTHTSEL "0: Through" - "any interrupt signal input to the corresponding
               GPIO pin is not held and is instead allowed to pass through" (VR4121 UM
               19.2.13/19.2.14, VR4102 UM 18.2.13/18.2.14). */
            stat = static_cast<uint16_t>(stat & ~bit);
        }
        DriveIcuLocked();
    }

    void SaveState(StateWriter& w) override {
        SaveRegisters(w);
        for (const auto& d : pin_device_states_) d.save(w);
    }

    void RestoreState(StateReader& r) override {
        RestoreRegisters(r);
        for (const auto& d : pin_device_states_) d.restore(r);
    }

    void PostRestore() override {
        std::lock_guard<std::mutex> lk(mtx_);
        if constexpr (M.power_down_high != 0u) suspended_ = channel_->Suspended();
        DriveIcuLocked();
    }

private:
    void SaveRegisters(StateWriter& w) {
        std::lock_guard<std::mutex> lk(mtx_);
        w.Write("iosel_l", iosel_l_);    w.Write("iosel_h", iosel_h_);
        w.Write("piod_out_l", piod_out_l_); w.Write("piod_out_h", piod_out_h_);
        w.Write("podat_l", podat_l_);
        w.Write("podat_h", podat_h_);
        w.Write("intstat_l", intstat_l_);  w.Write("intstat_h", intstat_h_);
        w.Write("inten_l", inten_l_);    w.Write("inten_h", inten_h_);
        w.Write("inttyp_l", inttyp_l_);   w.Write("inttyp_h", inttyp_h_);
        w.Write("intalsel_l", intalsel_l_); w.Write("intalsel_h", intalsel_h_);
        w.Write("inthtsel_l", inthtsel_l_); w.Write("inthtsel_h", inthtsel_h_);
        if constexpr (!M.intstat_sets_while_disabled) {
            w.Write("retained_l", retained_l_);
            w.Write("retained_h", retained_h_);
        }
        w.Write("level", level_);
        w.Write<uint8_t>("hibernating", hibernating_ ? 1u : 0u);
    }

    void RestoreRegisters(StateReader& r) {
        std::lock_guard<std::mutex> lk(mtx_);
        r.Read("iosel_l", iosel_l_);    r.Read("iosel_h", iosel_h_);
        r.Read("piod_out_l", piod_out_l_); r.Read("piod_out_h", piod_out_h_);
        r.Read("podat_l", podat_l_);
        r.Read("podat_h", podat_h_);
        r.Read("intstat_l", intstat_l_);  r.Read("intstat_h", intstat_h_);
        r.Read("inten_l", inten_l_);    r.Read("inten_h", inten_h_);
        r.Read("inttyp_l", inttyp_l_);   r.Read("inttyp_h", inttyp_h_);
        r.Read("intalsel_l", intalsel_l_); r.Read("intalsel_h", intalsel_h_);
        r.Read("inthtsel_l", inthtsel_l_); r.Read("inthtsel_h", inthtsel_h_);
        if constexpr (!M.intstat_sets_while_disabled) {
            r.Read("retained_l", retained_l_);
            r.Read("retained_h", retained_h_);
        }
        r.Read("level", level_);
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
        return (out & sel) | (level_ & ~sel);
    }

    uint16_t ReadHalfLocked(uint32_t off) {
        switch (off) {
            case kOffIoSelL: return iosel_l_;
            case kOffIoSelH: return iosel_h_;
            case kOffPiodL:     return static_cast<uint16_t>(EffectivePinsLocked());
            case kOffPiodH:     return static_cast<uint16_t>(EffectivePinsLocked() >> 16);
            case kOffIntStatL:  return intstat_l_;
            case kOffIntStatH:  return intstat_h_;
            case kOffIntEnL:    return inten_l_;
            case kOffIntEnH:    return inten_h_;
            case kOffIntTypL:   return inttyp_l_;
            case kOffIntTypH:   return inttyp_h_;
            case kOffIntAlSelL: return intalsel_l_;
            case kOffIntAlSelH: return intalsel_h_;
            case kOffIntHtSelL: return inthtsel_l_;
            case kOffIntHtSelH: return inthtsel_h_;
            case kOffPoDatL:    return podat_l_;
            case kOffPoDatH:    return podat_h_;
            default: HaltUnsupportedAccess("VR41xx GIU ReadHalf", M.base + off, 0);
        }
    }

    void WriteHalfLocked(uint32_t off, uint16_t value) {
        switch (off) {
            case kOffIoSelL: iosel_l_ = static_cast<uint16_t>(value & kIoSelLWritable); return;
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

            /* GIUINTSTAT INTS: "Cleared to 0 when 1 is written" (VR4121 UM 19.2.5/19.2.6,
               VR4102 UM 18.2.5/18.2.6), and "any held interrupt signal is cleared when '1'
               is set to the corresponding bit in the GIUINTSTAT register" (VR4121 UM
               19.2.13/19.2.14, VR4102 UM 18.2.13/18.2.14). */
            case kOffIntStatL:
                intstat_l_  = static_cast<uint16_t>(intstat_l_ & ~value);
                retained_l_ = static_cast<uint16_t>(retained_l_ & ~value);
                DriveIcuLocked();
                return;
            case kOffIntStatH:
                intstat_h_  = static_cast<uint16_t>(intstat_h_ & ~value);
                retained_h_ = static_cast<uint16_t>(retained_h_ & ~value);
                DriveIcuLocked();
                return;

            /* "An interrupt still occurs when the interrupt enable bit is again set to
               enable interrupts" (VR4102 UM 18.2.13/18.2.14). */
            case kOffIntEnL: {
                const uint16_t promote = static_cast<uint16_t>(retained_l_ & value);
                intstat_l_  |= promote;
                retained_l_  = static_cast<uint16_t>(retained_l_ & ~promote);
                inten_l_     = value;
                DriveIcuLocked();
                return;
            }
            case kOffIntEnH: {
                const uint16_t promote = static_cast<uint16_t>(retained_h_ & value);
                intstat_h_  |= promote;
                retained_h_  = static_cast<uint16_t>(retained_h_ & ~promote);
                inten_h_     = value;
                DriveIcuLocked();
                return;
            }

            case kOffIntTypL:   inttyp_l_   = value; return;
            case kOffIntTypH:   inttyp_h_   = value; return;
            case kOffIntAlSelL: intalsel_l_ = value; return;
            case kOffIntAlSelH: intalsel_h_ = value; return;
            case kOffIntHtSelL: inthtsel_l_ = value; return;
            case kOffIntHtSelH: inthtsel_h_ = value; return;

            default: HaltUnsupportedAccess("VR41xx GIU WriteHalf", M.base + off, value);
        }
    }

    /* Every GIU register's After-reset row is 0 except GIUPODATL's and GIUPODATH's: on the
       VR4121 both read "Previous value is retained" (UM 19.2.1-19.2.16), while on the
       VR4102 GIUPODATL is all-1 and GIUPODATH is 0 (UM 18.2.1-18.2.16). GIUPIOD's input
       bits track the pin, which no reset drives. */
    void ApplyResetLocked(ResetLineKind kind) {
        iosel_l_    = 0; iosel_h_    = 0;
        piod_out_l_ = 0; piod_out_h_ = 0;
        intstat_l_  = 0; intstat_h_  = 0;
        inten_l_    = 0; inten_h_    = 0;
        inttyp_l_   = 0; inttyp_h_   = 0;
        intalsel_l_ = 0; intalsel_h_ = 0;
        inthtsel_l_ = 0; inthtsel_h_ = 0;
        if constexpr (!M.intstat_sets_while_disabled) {
            retained_l_ = 0;
            retained_h_ = 0;
        }
        if (kind == ResetLineKind::Rtc || !M.podat_l_retained_on_reset) {
            podat_l_ = M.podat_l_power_on;
        }
        if (kind == ResetLineKind::Rtc || !M.podat_h_retained_on_reset) {
            podat_h_ = 0;
        }
        DriveIcuLocked();
    }

    /* The ICU's GIUINTLREG/GIUINTHREG are read-only INTS(15:0), "Interrupt to GPIO pin.
       1: Occurred / 0: Normal" (VR4121 UM 15.2.5, VR4102 UM 14.2.5). */
    void DriveIcuLocked() {
        auto& icu = emu_.Get<Vr41xxIcu>();
        if constexpr (M.inten_gates_icu_input) {
            icu.SetGiuLow(static_cast<uint16_t>(intstat_l_ & inten_l_));
            icu.SetGiuHigh(static_cast<uint16_t>(intstat_h_ & inten_h_));
        } else {
            icu.SetGiuLow(intstat_l_);
            icu.SetGiuHigh(intstat_h_);
        }
    }

    void NotifyOutputDataLow(uint16_t previous) {
        for (auto& fn : podat_l_listeners_) fn(previous);
    }

    static Vr41xxPinDrive Level(uint32_t bit) {
        return bit != 0u ? Vr41xxPinDrive::kHigh : Vr41xxPinDrive::kLow;
    }

    static bool SetsOutputDrive(uint32_t off) {
        return off == kOffIoSelL || off == kOffIoSelH || off == kOffPiodL || off == kOffPiodH ||
               off == kOffPoDatL || off == kOffPoDatH;
    }

    struct Alternate {
        int               first;
        int               last;
        AlternateFunction fn;
    };

    struct PinDeviceState {
        std::function<void(StateWriter&)> save;
        std::function<void(StateReader&)> restore;
    };
    std::vector<PinDeviceState> pin_device_states_;

    mutable std::mutex mtx_;
    Vr41xxPmu*         pmu_ = nullptr;
    cerfengine::MipsInterruptChannel* channel_ = nullptr;
    bool               hibernating_ = false;
    bool               suspended_   = false;
    std::vector<std::function<void(uint16_t)>> podat_l_listeners_;
    std::vector<Alternate>                     alternates_;
    std::vector<std::function<void()>>         drive_listeners_;

    /* Every GIU register's RTCRST column is 0 except GIUPODATL's (VR4121 UM
       19.2.1-19.2.16, VR4102 UM 18.2.1-18.2.16). */
    uint16_t iosel_l_    = 0, iosel_h_    = 0;
    uint16_t piod_out_l_ = 0, piod_out_h_ = 0;
    uint16_t podat_l_    = M.podat_l_power_on;
    uint16_t podat_h_    = 0;
    uint16_t intstat_l_  = 0, intstat_h_  = 0;
    uint16_t inten_l_    = 0, inten_h_    = 0;
    uint16_t inttyp_l_   = 0, inttyp_h_   = 0;
    uint16_t intalsel_l_ = 0, intalsel_h_ = 0;
    uint16_t inthtsel_l_ = 0, inthtsel_h_ = 0;
    uint16_t retained_l_ = 0, retained_h_ = 0;
    uint32_t level_      = 0;
};

}  /* namespace cerf_vr41xx_giu_detail */

}

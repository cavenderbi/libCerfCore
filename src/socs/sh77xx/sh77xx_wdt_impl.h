#pragma once

#include <cstdint>
#include <string_view>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include <cerfengine/sh/sh_cpu.h>
#include "../../execution/engine_bridge.h"
#include "../../execution/guest_cycle_clock.h"
#include "../cycle_anchored_counter.h"
#include "../reg_window_impl.h"
#include "sh77xx_frqcr.h"
#include "sh77xx_intc.h"
#include "sh77xx_peripheral_clock_ticks.h"
#include "sh77xx_wdt.h"

namespace cerfcore {

namespace sh77xx_wdt {

using cerf_reg_window_detail::OtherReset;
using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegSpec;
using cerf_reg_window_detail::RegWindowBase;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

inline constexpr uint32_t kBase = 0x1FFFFF84u;

inline constexpr uint32_t kWtcnt = 0u;
inline constexpr uint32_t kWtcsr = 1u;

inline constexpr uint16_t kWtcntKey   = 0x5Au;
inline constexpr uint16_t kWtcsrKey   = 0xA5u;
inline constexpr uint16_t kData       = 0x00FFu;
inline constexpr uint16_t kWtcsrTme   = 0x0080u;
inline constexpr uint16_t kWtcsrWt    = 0x0040u;
inline constexpr uint16_t kWtcsrRsts  = 0x0020u;
inline constexpr uint16_t kWtcsrWovf  = 0x0010u;
inline constexpr uint16_t kWtcsrIovf  = 0x0008u;
inline constexpr uint16_t kWtcsrCks   = 0x0007u;
inline constexpr uint16_t kWtcsrClock = kWtcsrWt | kWtcsrCks;

inline constexpr uint32_t kCksShift[8]         = {0u, 2u, 4u, 5u, 6u, 8u, 10u, 12u};
inline constexpr uint64_t kCounterSpan         = 0x100u;
inline constexpr uint64_t kStandbyReentryCount = 0x80u;

enum class Count : uint8_t { kNone, kStandbyExit, kWatchdog, kInterval };

constexpr RegSpec Register() {
    return RegSpec{ReadKind::kFatal, WriteKind::kStored, kData, 0u, 0u, OtherReset::kRetain};
}

constexpr RegWindowModel WindowOf() {
    RegWindowModel w{kBase, 4u, 2u, false, {}};
    w.reg[kWtcnt] = Register();
    w.reg[kWtcsr] = Register();
    return w;
}

inline constexpr RegWindowModel kWindow = WindowOf();

template <const std::string_view& Soc, bool kCountsAfterStandbyResume = true>
class Sh77xxWdtBase : public RegWindowBase<Soc, kWindow, Sh77xxWdt, true> {
    using Base = RegWindowBase<Soc, kWindow, Sh77xxWdt, true>;

public:
    using Base::Base;

    void OnReady() override {
        Base::OnReady();
        clock_ = &this->emu_.template Get<GuestCycleClock>();
        cpu_   = &this->emu_.template Get<EngineBridge>().Engine().template Get<cerfengine::ShCpu>();
        intc_  = &this->emu_.template Get<Sh77xxIntc>();
        frqcr_ = this->emu_.template TryGet<Sh77xxFrqcr>();
        event_ = clock_->Add([this] { OnCountEnd(); });
        if (frqcr_ != nullptr) {
            pclk_.Attach([this] { Retime(); });
            frqcr_->RegisterChangeListener([this](uint16_t from, uint16_t to) { OnFrequencyChange(from, to); });
        }
        GuestCpuReset& reset = this->emu_.template Get<GuestCpuReset>();
        reset.RegisterResetListener([this](ResetLineKind kind) { OnReset(kind); });
        reset.RegisterResetReleaseListener([this] { DriveIti(); });
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t reg = (addr - kBase) / 2u;
        const uint16_t key = reg == kWtcnt ? kWtcntKey : kWtcsrKey;
        if ((value >> 8) != key) {
            LOG(SocWdt, "Sh77xxWdt: write of 0x%04X at 0x%08X without the register key is not performed\n", value, addr);
            return;
        }
        if (count_ == Count::kStandbyExit) {
            this->emu_.template Get<Fatal>().Die("Sh77xxWdt: write of 0x%04X at 0x%08X while the WDT counts after a standby exit",
                                                 value, addr);
        }
        const uint16_t data = static_cast<uint16_t>(value & kData);
        if (Counting()) {
            WriteWhileCounting(addr, reg, data);
            DriveIti();
            return;
        }
        Base::WriteHalf(addr, data);
        if (reg == kWtcnt) settling_count_ran_ = false;
        if (reg == kWtcsr && (data & kWtcsrTme) != 0u) {
            if (settling_count_ran_) {
                this->emu_.template Get<Fatal>().Die(
                    "Sh77xxWdt: WTCSR write of 0x%02X starts the timer from the WTCNT of a clock settling count", data);
            }
            StartTimer(this->StoredReg(kWtcnt));
        }
        DriveIti();
    }

    void CountAfterStandby() override {
        if (Counting()) {
            this->emu_.template Get<Fatal>().Die("Sh77xxWdt: a standby exit while WTCSR 0x%02X has TME set",
                                                 this->StoredReg(kWtcsr));
        }
        if constexpr (kCountsAfterStandbyResume) {
            StartCount(Count::kStandbyExit, 0u);
        } else {
            settling_count_ran_ = true;
        }
    }

    void StopStandbyCount() override {
        if (count_ == Count::kStandbyExit) Disarm();
    }

    void SaveState(StateWriter& w) override {
        Base::SaveState(w);
        if (frqcr_ != nullptr) pclk_.Save(w);
        w.Write("count_kind", static_cast<uint8_t>(count_));
        w.Write("live_count", count_ == Count::kNone ? uint16_t{0u} : CurrentCount());
        w.Write("settling_count_ran", settling_count_ran_);
    }

    void RestoreState(StateReader& r) override {
        Base::RestoreState(r);
        if (frqcr_ != nullptr) pclk_.Restore(r);
        uint8_t count = 0u;
        r.Read("count_kind", count);
        count_ = static_cast<Count>(count);
        r.Read("live_count", restored_count_);
        r.Read("settling_count_ran", settling_count_ran_);
    }

    void PostRestore() override {
        if (frqcr_ != nullptr) pclk_.PostRestore();
        DriveIti();
        if (count_ == Count::kNone) {
            clock_->Disarm(event_);
            return;
        }
        counter_.Anchor(DividedTicks(), restored_count_);
        Arm();
    }

private:
    bool Counting() const { return count_ == Count::kWatchdog || count_ == Count::kInterval; }

    void DriveIti() { intc_->SetSourceRequest(Sh77xxIntcSource::kIti, (this->StoredReg(kWtcsr) & kWtcsrIovf) != 0u); }

    void WriteWhileCounting(uint32_t addr, uint32_t reg, uint16_t data) {
        if (reg == kWtcnt) {
            Base::WriteHalf(addr, data);
            StartTimer(data);
            return;
        }
        if ((data & kWtcsrTme) == 0u) {
            this->SetStoredReg(kWtcnt, CurrentCount());
            Disarm();
            Base::WriteHalf(addr, data);
            return;
        }
        const bool     clock_change = ((data ^ this->StoredReg(kWtcsr)) & kWtcsrClock) != 0u;
        const uint16_t count        = CurrentCount();
        Base::WriteHalf(addr, data);
        if (clock_change) StartTimer(count);
    }

    void StartTimer(uint16_t value) {
        StartCount((this->StoredReg(kWtcsr) & kWtcsrWt) != 0u ? Count::kWatchdog : Count::kInterval, value);
    }

    void StartCount(Count mode, uint16_t value) {
        if (frqcr_ == nullptr) {
            this->emu_.template Get<Fatal>().Die("Sh77xxWdt: counting on %.*s needs FRQCR, and no FRQCR model exists for it",
                                                 static_cast<int>(Soc.size()), Soc.data());
        }
        count_ = mode;
        counter_.Anchor(DividedTicks(), value);
        Arm();
    }

    uint32_t Shift() const { return kCksShift[this->StoredReg(kWtcsr) & kWtcsrCks]; }

    uint64_t DividedTicks() const { return pclk_.DividedTicks(Shift()); }

    uint16_t CurrentCount() const { return static_cast<uint16_t>(counter_.CountAt(DividedTicks()) & kData); }

    uint64_t EndTick() const {
        const uint64_t end = Counting() ? kCounterSpan : kStandbyReentryCount;
        return counter_.CycleOfTick(end - counter_.AnchorCount());
    }

    void Arm() { clock_->Arm(event_, pclk_.CycleOfDividedTick(EndTick(), Shift())); }

    void Disarm() {
        count_ = Count::kNone;
        clock_->Disarm(event_);
    }

    void Retime() {
        if (count_ != Count::kNone) Arm();
    }

    void OnFrequencyChange(uint16_t from, uint16_t to) {
        if (from == to) return;
        if (count_ != Count::kNone) {
            this->emu_.template Get<Fatal>().Die("Sh77xxWdt: FRQCR changed from 0x%04X to 0x%04X while the WDT counts",
                                                 from, to);
        }
        if (((from ^ to) & kSh77xxFrqcrPllCircuit1) != 0u && !this->emu_.template Get<GuestCpuReset>().Delivering()) {
            settling_count_ran_ = true;
        }
    }

    void OnReset(ResetLineKind kind) {
        const bool power_on = this->IsPowerOnReset(kind);
        if (count_ == Count::kStandbyExit || power_on) Disarm();
        if (power_on) settling_count_ran_ = false;
    }

    void OnCountEnd() {
        const uint16_t csr = this->StoredReg(kWtcsr);
        if (count_ == Count::kStandbyExit) {
            this->emu_.template Get<Fatal>().Die(
                "Sh77xxWdt: WTCNT reached H'80 after a standby exit with STBCR.STBY set (WTCSR CKS %u)", csr & kWtcsrCks);
        }
        if (count_ == Count::kInterval) {
            this->SetStoredReg(kWtcsr, static_cast<uint16_t>(csr | kWtcsrIovf));
            counter_.Anchor(EndTick(), 0u);
            Arm();
            DriveIti();
            return;
        }
        this->SetStoredReg(kWtcsr, static_cast<uint16_t>(csr | kWtcsrWovf));
        counter_.Anchor(EndTick(), 0u);
        Arm();
        cpu_->SetPendingResetKind((csr & kWtcsrRsts) != 0u ? cerfengine::ShResetKind::kManual
                                                           : cerfengine::ShResetKind::kPowerOn);
        this->emu_.template Get<GuestCpuReset>().ResetWithLatchedCause(ResetLineKind::Watchdog);
    }

    GuestCycleClock*           clock_ = nullptr;
    GuestCycleClock::Event*    event_ = nullptr;
    cerfengine::ShCpu*         cpu_   = nullptr;
    Sh77xxIntc*                intc_  = nullptr;
    Sh77xxFrqcr*               frqcr_ = nullptr;
    Sh77xxPeripheralClockTicks pclk_{this->emu_};
    TickAnchoredCounter        counter_;
    uint16_t                   restored_count_ = 0u;
    Count                      count_          = Count::kNone;
    bool                       settling_count_ran_ = false;
};

}

}

#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfengine/sh/sh_interrupt_channel.h>
#include <cerfengine/sh/sh_sleep_host.h>
#include "../../execution/engine_bridge.h"
#include "../reg_window_impl.h"
#include "sh77xx_standby_control.h"
#include "sh77xx_standby_entry.h"
#include "sh77xx_wdt.h"
#include <cerfcore/core/callback_list.h>

namespace cerfcore {

namespace sh77xx_power_down {

using cerf_reg_window_detail::OtherReset;
using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegSpec;
using cerf_reg_window_detail::RegWindowBase;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

inline constexpr uint32_t kStbcrPa  = 0x1FFFFF82u;
inline constexpr uint32_t kStbcr2Pa = 0x1FFFFF88u;

inline constexpr uint16_t kStbcrWritable    = 0x87u;
inline constexpr uint16_t kStbcrStby        = 0x80u;
inline constexpr uint8_t  kStbcrModuleStops = 0x07u;

constexpr RegWindowModel ByteRegister(uint32_t pa, uint16_t writable, uint16_t write_zero, OtherReset other_reset) {
    RegWindowModel w{pa, 1u, 1u, false, {}, 1u};
    w.reg[0] = RegSpec{ReadKind::kStored, WriteKind::kStored, writable, 0u, write_zero, other_reset};
    return w;
}

constexpr RegWindowModel Stbcr(uint16_t write_zero, OtherReset other_reset) {
    return ByteRegister(kStbcrPa, kStbcrWritable, write_zero, other_reset);
}

constexpr RegWindowModel Stbcr2(uint16_t writable, uint16_t write_zero, OtherReset other_reset) {
    return ByteRegister(kStbcr2Pa, writable, write_zero, other_reset);
}

template <const std::string_view& Soc, const RegWindowModel& M, class Slot, uint8_t kModuleStops>
class ModuleStopRegister : public RegWindowBase<Soc, M, Slot> {
    using Base = RegWindowBase<Soc, M, Slot>;

public:
    using Base::Base;

    bool ModuleStopped(uint8_t mstp) const override { return (Stopped() & mstp) != 0u; }

    void RegisterChangeListener(typename Slot::ChangeListener listener) override {
        listeners_.Add(std::move(listener));
    }

    void SaveState(StateWriter& w) override {
        Base::SaveState(w);
        w.Write("mstp_released_by_reset", released_);
    }

    void RestoreState(StateReader& r) override {
        Base::RestoreState(r);
        r.Read("mstp_released_by_reset", released_);
    }

protected:
    void AfterReset() override { released_ = Mstp(); }

    void BeforeStore(uint32_t, uint16_t) override { before_ = Stopped(); }

    void AfterStore(uint32_t) override {
        released_           = 0u;
        const uint8_t after = Stopped();
        if (after != before_) listeners_.Call(before_, after);
    }

private:
    uint8_t Mstp() const { return static_cast<uint8_t>(this->StoredReg(0u) & kModuleStops); }
    uint8_t Stopped() const { return static_cast<uint8_t>(Mstp() & ~released_); }

    CallbackList<uint8_t, uint8_t> listeners_;
    uint8_t                        before_   = 0u;
    uint8_t                        released_ = 0u;
};

template <const std::string_view& Soc, const RegWindowModel& M, bool kExitClearsStby = false>
class StandbyControlRegister : public ModuleStopRegister<Soc, M, Sh77xxStbcr, kStbcrModuleStops>,
                               public cerfengine::ShSleepHost,
                               public DeepSleepClockStop {
    using Stop = ModuleStopRegister<Soc, M, Sh77xxStbcr, kStbcrModuleStops>;

public:
    using Stop::Stop;

    void OnReady() override {
        Stop::OnReady();
        channel_ = &this->emu_.template Get<EngineBridge>().Engine().template Get<cerfengine::ShInterruptChannel>();
        sleep_   = &this->emu_.template Get<GuestDeepSleep>();
        wdt_     = &this->emu_.template Get<Sh77xxWdt>();
        channel_->SetSleepHost(this);
        sleep_->RegisterClockStopWaker(this);
        this->emu_.template Get<Sh77xxStandbyEntry>().RegisterWakeCheck(
            [this] { detected_ = channel_->StandbyWakePending(); });
        sleep_->RegisterParkWakeSource([this] { return Detect(); });
        sleep_->RegisterParkWakeDue([this] {
            return detected_ ? sleep_->SleptNs() : GuestDeepSleep::kNoParkWake;
        });
    }

    bool StandbySelected() const override { return (this->StoredReg(0u) & kStbcrStby) != 0u; }

    void EnterStandby() override {
        exited_ = false;
        sleep_->Enter();
    }

    bool StandbyExited() const override { return exited_; }

    void OnPowerUp() override {
        exited_ = Detect();
        if (!exited_) return;
        if constexpr (kExitClearsStby) {
            this->SetStoredReg(0u, static_cast<uint16_t>(this->StoredReg(0u) & ~kStbcrStby));
        }
        wdt_->CountAfterStandby();
    }

    void SaveState(StateWriter& w) override {
        Stop::SaveState(w);
        w.Write("standby_wake_detected", detected_);
        w.Write("standby_exited", exited_);
    }

    void RestoreState(StateReader& r) override {
        Stop::RestoreState(r);
        r.Read("standby_wake_detected", detected_);
        r.Read("standby_exited", exited_);
    }

protected:
    void AfterStore(uint32_t i) override {
        Stop::AfterStore(i);
        if (!StandbySelected()) wdt_->StopStandbyCount();
    }

private:
    bool Detect() {
        if (!detected_) detected_ = channel_->StandbyWakePending();
        return detected_;
    }

    cerfengine::ShInterruptChannel* channel_  = nullptr;
    GuestDeepSleep*                 sleep_    = nullptr;
    Sh77xxWdt*                      wdt_      = nullptr;
    bool                            detected_ = false;
    bool                            exited_   = false;
};

}

}

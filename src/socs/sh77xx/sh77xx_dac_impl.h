#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../reg_window_impl.h"
#include "sh77xx_dac.h"
#include "sh77xx_standby_control.h"

namespace cerfcore {

namespace sh77xx_dac {

using cerf_reg_window_detail::OtherReset;
using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegSpec;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

inline constexpr uint32_t kDadr0 = 0u;
inline constexpr uint32_t kDadr1 = 2u;
inline constexpr uint32_t kDacr  = 4u;

inline constexpr uint16_t kDaoe1 = 0x80u;
inline constexpr uint16_t kDaoe0 = 0x40u;

constexpr RegWindowModel WindowOf(OtherReset other_reset) {
    RegWindowModel w{kSh77xxDacBase, 5u, 5u, false, {}, 1u};
    w.reg[kDadr0] = RegSpec{ReadKind::kStored, WriteKind::kStored, 0xFFu, 0x00u, 0x00u, other_reset};
    w.reg[kDadr1] = RegSpec{ReadKind::kStored, WriteKind::kStored, 0xFFu, 0x00u, 0x00u, other_reset};
    w.reg[kDacr]  = RegSpec{ReadKind::kStored, WriteKind::kStored, 0xE0u, 0x1Fu, 0x00u, other_reset};
    return w;
}

template <const std::string_view& Soc, const RegWindowModel& W, uint8_t kStbcr2ModuleStop>
class Sh77xxDacBase : public cerf_reg_window_detail::RegWindowBase<Soc, W, Sh77xxDac> {
    using Base = cerf_reg_window_detail::RegWindowBase<Soc, W, Sh77xxDac>;

public:
    using Base::Base;

    void OnReady() override {
        Base::OnReady();
        stbcr2_ = &this->emu_.template Get<Sh77xxStbcr2>();
        stbcr2_->RegisterChangeListener([this](uint8_t from, uint8_t to) { OnModuleStopChange(from, to); });
        this->emu_.template Get<GuestCpuReset>().RegisterResetReleaseListener([this] { this->NotifyOutputChanged(); });
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        if (value != 0u) this->HaltUnsupportedAccess("WriteHalf of a nonzero value to an 8-bit D/A register", addr, value);
        this->WriteByte(addr, 0u);
    }

    Sh77xxDac::Output ChannelOutput(uint32_t channel) const override {
        if (stbcr2_->ModuleStopped(kStbcr2ModuleStop)) return held_[channel];
        return LiveOutput(channel);
    }

    void SaveState(StateWriter& w) override {
        Base::SaveState(w);
        for (const Sh77xxDac::Output& held : held_) {
            w.Write("held_enabled", held.enabled);
            w.Write("held_value", held.value);
        }
    }

    void RestoreState(StateReader& r) override {
        Base::RestoreState(r);
        for (Sh77xxDac::Output& held : held_) {
            r.Read("held_enabled", held.enabled);
            r.Read("held_value", held.value);
        }
        this->NotifyRestored();
    }

protected:
    void AfterStore(uint32_t) override { this->NotifyOutputChanged(); }

private:
    Sh77xxDac::Output LiveOutput(uint32_t channel) const {
        const uint16_t enable = channel == 0u ? kDaoe0 : kDaoe1;
        return Sh77xxDac::Output{(this->StoredReg(kDacr) & enable) != 0u,
                                 static_cast<uint8_t>(this->StoredReg(channel == 0u ? kDadr0 : kDadr1))};
    }

    void OnModuleStopChange(uint8_t from, uint8_t to) {
        const bool was_stopped = (from & kStbcr2ModuleStop) != 0u;
        const bool stopped     = (to & kStbcr2ModuleStop) != 0u;
        if (!was_stopped && stopped) {
            held_[0] = LiveOutput(0u);
            held_[1] = LiveOutput(1u);
        }
        if (was_stopped && !stopped) this->NotifyOutputChanged();
    }

    Sh77xxStbcr2*                    stbcr2_ = nullptr;
    std::array<Sh77xxDac::Output, 2> held_{};
};

}

}

#include "../vr41xx/vr41xx_rtc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include <cerfengine/mips/mips_interrupt_channel.h>
#include "../../cpu/mips/mips_core_clock.h"
#include "../../execution/engine_bridge.h"
#include "vr4101_id.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

namespace {

constexpr uint32_t kOffTclkL  = 0x18u;
constexpr uint32_t kOffTclkH  = 0x1Au;
constexpr uint32_t kOffRtcInt = 0x1Cu;

constexpr uint16_t kVr4101IntRtcLong = 1u << 0;
constexpr uint16_t kVr4101IntElapsed = 1u << 1;

constexpr uint16_t kTclkEnable    = 0x8000u;
constexpr uint32_t kTclkCountMask = 0x7FFFFFFFu;

constexpr Vr41xxRtcModel kModel{4u, false, false, 2u};

class Vr4101TclkCounter {
public:
    explicit Vr4101TclkCounter(CerfEmulator& emu) : emu_(emu) {}

    void Attach() {
        channel_    = &emu_.Get<EngineBridge>().Engine().Get<cerfengine::MipsInterruptChannel>();
        core_clock_ = &emu_.Get<MipsCoreClock>();
    }

    void Reset() {
        std::lock_guard<std::mutex> lk(mtx_);
        enabled_ = false;
        base_    = 0u;
    }

    uint16_t ReadHalf(bool high) {
        std::lock_guard<std::mutex> lk(mtx_);
        const uint32_t count = CountLocked();
        if (!high) return static_cast<uint16_t>(count & 0xFFFFu);
        return static_cast<uint16_t>((enabled_ ? kTclkEnable : 0u) | ((count >> 16) & 0x7FFFu));
    }

    void WriteHalf(bool high, uint16_t value) {
        std::lock_guard<std::mutex> lk(mtx_);
        const uint32_t count = CountLocked();
        if (!high) {
            RebaseLocked((count & 0x7FFF0000u) | value);
            return;
        }
        if ((value & kTclkEnable) == 0u) {
            enabled_ = false;
            base_    = 0u;
            return;
        }
        if (!enabled_) cycles_per_tick_ = core_clock_->CyclesPerTclkCounterTick();
        enabled_ = true;
        RebaseLocked((static_cast<uint32_t>(value & 0x7FFFu) << 16) | (count & 0xFFFFu));
    }

    void Save(StateWriter& w) {
        std::lock_guard<std::mutex> lk(mtx_);
        w.Write<uint8_t>("tclk_enabled", enabled_ ? 1u : 0u);
        w.Write("tclk_count", CountLocked());
    }

    void Restore(StateReader& r) {
        std::lock_guard<std::mutex> lk(mtx_);
        uint8_t enabled = 0u;
        r.Read("tclk_enabled", enabled);
        r.Read("tclk_count", base_);
        enabled_ = enabled != 0u;
    }

    void Rearm() {
        std::lock_guard<std::mutex> lk(mtx_);
        cycles_per_tick_ = core_clock_->CyclesPerTclkCounterTick();
        anchor_          = channel_->CyclesOutsideSuspend();
    }

private:
    uint32_t CountLocked() const {
        if (!enabled_) return base_;
        const uint64_t ticks = (channel_->CyclesOutsideSuspend() - anchor_) / cycles_per_tick_;
        return static_cast<uint32_t>((base_ + ticks) & kTclkCountMask);
    }

    void RebaseLocked(uint32_t count) {
        base_   = count & kTclkCountMask;
        anchor_ = channel_->CyclesOutsideSuspend();
    }

    CerfEmulator&                     emu_;
    std::mutex                        mtx_;
    cerfengine::MipsInterruptChannel* channel_         = nullptr;
    MipsCoreClock*                    core_clock_      = nullptr;
    bool                              enabled_         = false;
    uint32_t                          base_            = 0u;
    uint64_t                          anchor_          = 0u;
    uint64_t                          cycles_per_tick_ = 1u;
};

class Vr4101Rtc : public Vr41xxRtc {
public:
    using Vr41xxRtc::Vr41xxRtc;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4101; }

    void OnReady() override {
        Vr41xxRtc::OnReady();
        tclk_.Attach();
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
            if (kind == ResetLineKind::Rtc) tclk_.Reset();
        });
    }

    const Vr41xxRtcModel& Model() const override { return kModel; }

    uint16_t ReadHalf(uint32_t addr) override {
        switch (addr - MmioBase()) {
            case kOffTclkL:  return tclk_.ReadHalf(false);
            case kOffTclkH:  return tclk_.ReadHalf(true);
            case kOffRtcInt: return ToVr4101Int(ReadHalf2(kRtcIntReg));
            default:         return Vr41xxRtc::ReadHalf(MmioBase() + SharedOffset(addr, 0u));
        }
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        switch (addr - MmioBase()) {
            case kOffTclkL: tclk_.WriteHalf(false, value); return;
            case kOffTclkH: tclk_.WriteHalf(true, value);  return;
            case kOffRtcInt:
                if ((value & ~(kVr4101IntRtcLong | kVr4101IntElapsed)) != 0u) {
                    HaltUnsupportedAccess("RTCINTREG write", addr, value);
                }
                WriteHalf2(kRtcIntReg, FromVr4101Int(value));
                return;
            default: Vr41xxRtc::WriteHalf(MmioBase() + SharedOffset(addr, value), value); return;
        }
    }

    void SaveState(StateWriter& w) override {
        Vr41xxRtc::SaveState(w);
        tclk_.Save(w);
    }

    void RestoreState(StateReader& r) override {
        Vr41xxRtc::RestoreState(r);
        tclk_.Restore(r);
    }

    void PostRestore() override {
        Vr41xxRtc::PostRestore();
        tclk_.Rearm();
    }

private:
    uint32_t SharedOffset(uint32_t addr, uint16_t value) {
        switch (addr - MmioBase()) {
            case 0x04u: return kEtimeL;
            case 0x06u: return kEtimeM;
            case 0x08u: return kEtimeH;
            case 0x0Au: return kEcmpH;
            case 0x0Cu: return kEcmpL;
            case 0x0Eu: return kEcmpM;
            case 0x10u: return kRtcl1L;
            case 0x12u: return kRtcl1H;
            case 0x14u: return kRtcl1CntL;
            case 0x16u: return kRtcl1CntH;
            default: HaltUnsupportedAccess("VR4101 RTC access", addr, value);
        }
    }

    static uint16_t ToVr4101Int(uint16_t shared) {
        return static_cast<uint16_t>(((shared & kIntLong1) != 0u ? kVr4101IntRtcLong : 0u) |
                                     ((shared & kIntElapsed) != 0u ? kVr4101IntElapsed : 0u));
    }

    static uint16_t FromVr4101Int(uint16_t value) {
        return static_cast<uint16_t>(((value & kVr4101IntRtcLong) != 0u ? kIntLong1 : 0u) |
                                     ((value & kVr4101IntElapsed) != 0u ? kIntElapsed : 0u));
    }

    Vr4101TclkCounter tclk_{emu_};
};

}

REGISTER_SERVICE_AS(Vr4101Rtc, Vr41xxRtc);

}

#include "../vr41xx/vr41xx_piu.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../cpu/mips/mips_core_clock.h"
#include "../../execution/guest_cycle_clock.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../oscillator_ticks.h"
#include "../vr41xx/vr41xx_cmu.h"
#include "../vr41xx/vr41xx_dma_page_interrupts.h"
#include "../vr41xx/vr41xx_icu.h"
#include "../vr41xx/vr41xx_piu_host_pen.h"
#include "../vr41xx/vr41xx_piu_regs.h"
#include "../vr41xx/vr41xx_piu_state_table.h"
#include "../vr41xx/vr41xx_unit_clock_gate.h"
#include "vr4101_dma_addresses.h"
#include "vr4101_dma_control.h"
#include "vr4101_id.h"
#include "vr4101_interval_unit.h"
#include "vr4101_piu_regs.h"
#include "vr4101_piu_sampler.h"

#include <algorithm>
#include <cstdint>
#include <mutex>

namespace cerfcore {

namespace {

using namespace vr4101_piu;

constexpr uint32_t kBase = 0x0B000120u;
constexpr uint32_t kSize = 0x20u;

constexpr uint32_t kOffData = 0x00u;
constexpr uint32_t kOffCnt  = 0x02u;
constexpr uint32_t kOffInt  = 0x04u;
constexpr uint32_t kOffSivl = 0x06u;
constexpr uint32_t kOffStbl = 0x08u;
constexpr uint32_t kOffCmd  = 0x0Au;
constexpr uint32_t kOffCivl = 0x1Cu;

constexpr uint16_t kIntChg  = 1u << 0;
constexpr uint16_t kIntRdy  = 1u << 1;
constexpr uint16_t kIntLost = 1u << 2;
constexpr uint16_t kIntPage = 1u << 3;
constexpr uint16_t kIntEnd  = 1u << 4;
constexpr uint16_t kIntAll  = 0x001Fu;

constexpr uint16_t kDataMask    = 0x0FFFu;
constexpr uint16_t kSivlMask    = 0x07FFu;
constexpr uint16_t kSivlReset   = 0x00A7u;
constexpr uint16_t kStblMask    = 0x01FFu;
constexpr uint16_t kStblReset   = 0x0077u;
constexpr uint16_t kSelAdClk    = 0x00F0u;
constexpr uint16_t kSelAdClkMin = 0x0020u;
constexpr uint16_t kCmdMask     = 0x03FFu;
constexpr uint16_t kCmdReset    = 0x01BEu;

constexpr Vr41xxDmaPageInterrupts kPages{kIntPage, kIntEnd};
constexpr Vr4101DmaChannel        kChannel = Vr4101DmaChannel::kPiu;

class Vr4101Piu : public Vr41xxPiu {
public:
    using Vr41xxPiu::Vr41xxPiu;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4101; }

    void OnReady() override;

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    uint32_t ReadWord(uint32_t addr) override { return ReadWordAsHalves(addr); }
    void     WriteWord(uint32_t addr, uint32_t value) override { WriteWordAsHalves(addr, value); }

    uint16_t ReadHalf(uint32_t addr) override {
        std::lock_guard<std::mutex> lk(mtx_);
        switch (addr - kBase) {
            case kOffData: return data_;
            case kOffCnt:  return CntValueLocked();
            case kOffInt:  return int_;
            case kOffSivl: return sivl_;
            case kOffStbl: return stbl_;
            case kOffCmd:  return cmd_;
            case kOffCivl: return CheckIntervalLocked();
            default:       HaltUnsupportedAccess("VR4101 PIU ReadHalf", addr, 0);
        }
    }

    void WriteHalf(uint32_t addr, uint16_t value) override;

    void SetPen(bool down, uint16_t pos_x, uint16_t pos_y) override {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            host_pen_.Pen(down, pos_x, pos_y);
        }
        host_pen_.Wake();
    }

    void SyntheticTap(uint16_t pos_x, uint16_t pos_y) override {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            host_pen_.Tap(pos_x, pos_y);
        }
        host_pen_.Wake();
    }

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

private:
    uint16_t CntValueLocked() const {
        return static_cast<uint16_t>(cnt_ | (penstp_ ? kPenStp : 0u) | (pen_.down ? kPenStc : 0u) |
                                     (state_ << kStateShift));
    }

    uint16_t CheckIntervalLocked() {
        if (state_ != kStInterval) return sivl_;
        const uint64_t units = (ticks_.Now() - interval_start_) / kVr4101TclocksPerIntervalUnit;
        return units >= sivl_ ? 0u : static_cast<uint16_t>(sivl_ - units);
    }

    uint64_t IntervalDueLocked() const {
        return interval_start_ + std::max<uint64_t>(static_cast<uint64_t>(sivl_) * kVr4101TclocksPerIntervalUnit,
                                                    sampler_.CoordinateSetTclocks(stbl_, cnt_));
    }

    uint16_t Mode() const { return static_cast<uint16_t>((cnt_ & kModeMask) >> kModeShift); }

    void WriteCntLocked(uint16_t value);
    void StartSequencerLocked();
    void BeginSetLocked(uint16_t scan_state);
    void RunSetLocked();
    void FinishSetLocked();
    void StopLocked(uint16_t next_state);
    void EnterWaitPenLocked();
    void EnterIntervalLocked();
    void TouchDetectedLocked();
    void ReleaseDetectedLocked();
    void PenEdgeLocked(bool down);
    void ApplyHostPenLocked();
    void IntervalEndLocked();
    void ArmLocked();
    void ResetLocked();
    void PublishLocked() {
        icu_->SetPiuSource(int_);
        dcu_->SetRequest(kChannel, stalled_);
    }
    void ApplyClockLocked();

    mutable std::mutex       mtx_;
    Vr41xxPiuHostPen         host_pen_{emu_};
    Vr4101PiuSampler         sampler_{emu_};
    Vr41xxUnitClockGate      gate_{emu_};
    StoppableOscillatorTicks ticks_{emu_, false};
    GuestCycleClock*         clock_ = nullptr;
    GuestCycleClock::Event*  event_ = nullptr;
    Vr4101DmaAddresses*      dma_   = nullptr;
    Vr4101DmaControl*        dcu_   = nullptr;
    Vr41xxIcu*               icu_   = nullptr;

    Vr41xxPiuPenPoint pen_{false, 0u, 0u};
    uint16_t cnt_    = 0u;
    uint16_t int_    = 0u;
    uint16_t data_   = 0u;
    uint16_t sivl_   = kSivlReset;
    uint16_t stbl_   = kStblReset;
    uint16_t cmd_    = kCmdReset;
    uint16_t state_  = kStDisable;
    uint16_t set_index_ = 0u;
    uint16_t synthetic_hold_ = 0u;
    uint64_t interval_start_ = 0u;
    bool     penstp_    = false;
    bool     stalled_   = false;
    bool     final_set_ = false;
};

void Vr4101Piu::OnReady() {
    emu_.Get<PeripheralDispatcher>().Register(this);
    dma_   = &emu_.Get<Vr4101DmaAddresses>();
    dcu_   = &emu_.Get<Vr4101DmaControl>();
    icu_   = &emu_.Get<Vr41xxIcu>();
    clock_ = &emu_.Get<GuestCycleClock>();

    auto&                       core     = emu_.Get<MipsCoreClock>();
    const GuestCycleClock::Rate pclock   = core.ResetRate();
    const uint64_t              tclk_den = pclock.den * core.CyclesPerTclkCounterTick();
    ticks_.Attach(pclock.num, tclk_den);
    sampler_.Attach(pclock.num, tclk_den);
    event_ = clock_->Add([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        IntervalEndLocked();
    });
    host_pen_.Attach([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        ApplyHostPenLocked();
    });
    dcu_->RegisterEnableListener(kChannel, [this] {
        std::lock_guard<std::mutex> lk(mtx_);
        if (stalled_) RunSetLocked();
    });
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
        std::lock_guard<std::mutex> lk(mtx_);
        ResetLocked();
    });
    gate_.Attach(kCmuMskPiu, [this] {
        std::lock_guard<std::mutex> lk(mtx_);
        ApplyClockLocked();
    });
    clock_->RegisterRateListener([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        ticks_.Rescale();
        ArmLocked();
    });
    gate_.Refresh();
}

void Vr4101Piu::WriteHalf(uint32_t addr, uint16_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    switch (addr - kBase) {
        case kOffData:
            if (value & ~kDataMask) HaltUnsupportedAccess("VR4101 PIU PIUDATAREG reserved bit", addr, value);
            data_ = value;
            return;
        case kOffCnt: WriteCntLocked(value); return;
        case kOffInt:
            if (value & ~kIntAll) HaltUnsupportedAccess("VR4101 PIU PIUINTREG reserved bit", addr, value);
            int_ = static_cast<uint16_t>(int_ & ~value);
            PublishLocked();
            return;
        case kOffSivl:
            if (value & ~kSivlMask) HaltUnsupportedAccess("VR4101 PIU PIUSIVLREG reserved bit", addr, value);
            sivl_ = value;
            ArmLocked();
            return;
        case kOffStbl:
            if ((value & ~kStblMask) || (value & kSelAdClk) < kSelAdClkMin) {
                HaltUnsupportedAccess("VR4101 PIU PIUSTBLREG reserved bit or RFU SELADCLK", addr, value);
            }
            stbl_ = value;
            ArmLocked();
            return;
        case kOffCmd:
            if (value & ~kCmdMask) HaltUnsupportedAccess("VR4101 PIU PIUCMDREG reserved bit", addr, value);
            cmd_ = value;
            return;
        default: HaltUnsupportedAccess("VR4101 PIU WriteHalf", addr, value);
    }
}

void Vr4101Piu::WriteCntLocked(uint16_t value) {
    if (value & kPadRst) {
        ResetLocked();
        return;
    }
    const uint16_t before = cnt_;
    cnt_    = static_cast<uint16_t>(value & kCntStored);
    penstp_ = (value & kPenStp) != 0u;
    if (!(cnt_ & kPiuPwr)) {
        StopLocked(kStDisable);
        PublishLocked();
        return;
    }
    if (state_ == kStDisable) state_ = kStStandby;
    if (!(cnt_ & kSeqEn) && (state_ == kStWaitPen || state_ == kStInterval)) StopLocked(kStStandby);
    if ((cnt_ & kSeqEn) && state_ == kStStandby) StartSequencerLocked();
    if ((cnt_ & kScanStop) && state_ == kStInterval) {
        cnt_ = static_cast<uint16_t>(cnt_ & ~(kScanStop | kSeqEn));
        StopLocked(kStStandby);
    }
    const bool at_start_rose = (cnt_ & kAtStart) && !(before & kAtStart);
    if (at_start_rose && state_ == kStWaitPen && pen_.down) BeginSetLocked(kStPenScan);
    PublishLocked();
}

void Vr4101Piu::StartSequencerLocked() {
    switch (Mode()) {
        case 0u:
            if (cnt_ & kScanStart) BeginSetLocked(kStPenScan);
            else EnterWaitPenLocked();
            return;
        case 1u: BeginSetLocked(kStCommand); return;
        case 2u: BeginSetLocked(kStMainBatt); return;
        default: BeginSetLocked(kStSubBatt); return;
    }
}

void Vr4101Piu::BeginSetLocked(uint16_t scan_state) {
    clock_->Disarm(event_);
    state_     = scan_state;
    set_index_ = 0u;
    RunSetLocked();
}

void Vr4101Piu::RunSetLocked() {
    stalled_ = false;
    const uint16_t length = sampler_.SetLength(state_, cnt_);
    for (; set_index_ < length; ++set_index_) {
        const Vr4101PiuSampler::Datum d = sampler_.SetDatum(state_, cnt_, cmd_, set_index_);
        const bool masked = ((cnt_ & kPadStop) && (int_ & kIntPage)) || (int_ & kIntEnd);
        if (!masked && !dcu_->TransferEnabled(kChannel)) {
            stalled_ = true;
            PublishLocked();
            return;
        }
        data_ = sampler_.Convert(d, stbl_, pen_);
        if (masked) {
            int_ |= kIntLost;
            continue;
        }
        if (dma_->Store(kChannel, data_)) int_ |= kPages.AtBoundary(int_);
    }
    int_ |= kIntRdy;
    FinishSetLocked();
    PublishLocked();
}

void Vr4101Piu::FinishSetLocked() {
    if (state_ != kStPenScan) {
        cnt_   = static_cast<uint16_t>(cnt_ & ~kSeqEn);
        state_ = kStStandby;
        return;
    }
    if (synthetic_hold_ != 0u && --synthetic_hold_ == 0u) PenEdgeLocked(false);
    if ((cnt_ & kScanStop) || !(cnt_ & kSeqEn)) {
        cnt_ = static_cast<uint16_t>(cnt_ & ~(kScanStop | kSeqEn));
        StopLocked(kStStandby);
        return;
    }
    if (!pen_.down) {
        if (!final_set_) int_ |= kIntChg;
        final_set_ = false;
        if (cnt_ & kAtStop) {
            state_ = kStWaitPen;
            return;
        }
    }
    EnterIntervalLocked();
}

void Vr4101Piu::StopLocked(uint16_t next_state) {
    clock_->Disarm(event_);
    stalled_   = false;
    final_set_ = false;
    state_     = next_state;
}

void Vr4101Piu::EnterWaitPenLocked() {
    state_ = kStWaitPen;
    if (cerf_vr41xx_piu_detail::PenChangeDetected(state_, pen_.down)) TouchDetectedLocked();
}

void Vr4101Piu::EnterIntervalLocked() {
    state_          = kStInterval;
    interval_start_ = ticks_.Now();
    ArmLocked();
}

void Vr4101Piu::TouchDetectedLocked() {
    int_ |= kIntChg;
    if (cnt_ & kAtStart) BeginSetLocked(kStPenScan);
}

void Vr4101Piu::ReleaseDetectedLocked() {
    int_ |= kIntChg;
    if (!(cnt_ & kAtStop)) return;
    final_set_ = true;
    BeginSetLocked(kStPenScan);
}

void Vr4101Piu::IntervalEndLocked() {
    if (state_ != kStInterval) return;
    if (ticks_.Now() < IntervalDueLocked()) {
        ArmLocked();
        return;
    }
    BeginSetLocked(kStPenScan);
}

void Vr4101Piu::ArmLocked() {
    if (state_ != kStInterval || !gate_.Running()) {
        clock_->Disarm(event_);
        return;
    }
    ticks_.ArmAt(event_, IntervalDueLocked());
}

void Vr4101Piu::PenEdgeLocked(bool down) {
    penstp_   = pen_.down;
    pen_.down = down;
    if (cerf_vr41xx_piu_detail::PenChangeDetected(state_, down)) {
        if (down) TouchDetectedLocked();
        else      ReleaseDetectedLocked();
    }
    PublishLocked();
}

void Vr4101Piu::ApplyHostPenLocked() {
    if (const std::optional<Vr41xxPiuPenPoint> p = host_pen_.TakePen()) {
        pen_.x          = p->x;
        pen_.y          = p->y;
        synthetic_hold_ = 0u;
        if (p->down != pen_.down) PenEdgeLocked(p->down);
    }
    if (const std::optional<Vr41xxPiuPenPoint> t = host_pen_.TakeTap(); t && !pen_.down) {
        pen_.x          = t->x;
        pen_.y          = t->y;
        synthetic_hold_ = cerf_vr41xx_piu_detail::kSyntheticHoldScans;
        PenEdgeLocked(true);
    }
}

void Vr4101Piu::ResetLocked() {
    clock_->Disarm(event_);
    cnt_ = int_ = data_ = 0u;
    sivl_  = kSivlReset;
    stbl_  = kStblReset;
    cmd_   = kCmdReset;
    state_ = kStDisable;
    set_index_ = synthetic_hold_ = 0u;
    penstp_ = stalled_ = final_set_ = false;
    PublishLocked();
}

void Vr4101Piu::ApplyClockLocked() {
    ticks_.SetCounting(gate_.Running());
    ArmLocked();
}

void Vr4101Piu::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(mtx_);
    w.Write<uint8_t>("pen_down", pen_.down ? 1u : 0u);
    w.Write("pen_x", pen_.x);
    w.Write("pen_y", pen_.y);
    w.Write("cnt", cnt_);
    w.Write("int", int_);
    w.Write("data", data_);
    w.Write("sivl", sivl_);
    w.Write("stbl", stbl_);
    w.Write("cmd", cmd_);
    w.Write("state", state_);
    w.Write("set_index", set_index_);
    w.Write("synthetic_hold", synthetic_hold_);
    w.Write("interval_start", interval_start_);
    w.Write<uint8_t>("penstp", penstp_ ? 1u : 0u);
    w.Write<uint8_t>("stalled", stalled_ ? 1u : 0u);
    w.Write<uint8_t>("final_set", final_set_ ? 1u : 0u);
    ticks_.Save(w, "piu_");
}

void Vr4101Piu::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(mtx_);
    uint8_t down = 0u, penstp = 0u, stalled = 0u, final_set = 0u;
    r.Read("pen_down", down);
    r.Read("pen_x", pen_.x);
    r.Read("pen_y", pen_.y);
    r.Read("cnt", cnt_);
    r.Read("int", int_);
    r.Read("data", data_);
    r.Read("sivl", sivl_);
    r.Read("stbl", stbl_);
    r.Read("cmd", cmd_);
    r.Read("state", state_);
    r.Read("set_index", set_index_);
    r.Read("synthetic_hold", synthetic_hold_);
    r.Read("interval_start", interval_start_);
    r.Read("penstp", penstp);
    r.Read("stalled", stalled);
    r.Read("final_set", final_set);
    ticks_.Restore(r, "piu_");
    pen_.down  = down != 0u;
    penstp_    = penstp != 0u;
    stalled_   = stalled != 0u;
    final_set_ = final_set != 0u;
    synthetic_hold_ = 0u;
    host_pen_.Clear();
    clock_->Disarm(event_);
}

void Vr4101Piu::PostRestore() {
    gate_.Refresh();
    std::lock_guard<std::mutex> lk(mtx_);
    if (pen_.down) PenEdgeLocked(false);
    PublishLocked();
}

}

REGISTER_SERVICE_AS(Vr4101Piu, Vr41xxPiu);

}

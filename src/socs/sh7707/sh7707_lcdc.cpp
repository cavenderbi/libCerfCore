#include "sh7707_lcdc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../sh77xx/sh77xx_cpg.h"
#include "../sh77xx/sh77xx_intc.h"
#include "../sh77xx/sh77xx_pfc.h"
#include "../sh77xx/sh77xx_port_data.h"
#include "sh7707_id.h"
#include "sh7707_lcd_panel.h"

namespace cerfcore {

namespace {

constexpr uint32_t kOffAr  = 0x0u;
constexpr uint32_t kOffDr  = 0x2u;
constexpr uint32_t kOffPr  = 0x6u;
constexpr uint32_t kOffDmr = 0xEu;

constexpr uint16_t kArMask    = 0x001Fu;
constexpr uint16_t kArIndex   = 0x000Fu;
constexpr uint16_t kPrMask    = 0x0FFFu;
constexpr uint32_t kDr13      = 13u;
constexpr uint16_t kDr13Vie   = 0x0001u;
constexpr uint16_t kDr0Don    = 0x0001u;
constexpr uint16_t kDr1Ren    = 0x0020u;
constexpr uint16_t kLineMask  = 0x03FFu;
constexpr uint16_t kDmr4Lem   = 0x8000u;
constexpr uint16_t kDmr4Tm    = 0x3FFFu;
constexpr uint32_t kLmDual     = Sh7707Lcdc::kModeDual;
constexpr uint32_t kLmTft      = Sh7707Lcdc::kModeTft;
constexpr uint32_t kLmStnColor = Sh7707Lcdc::kModeStnColor;
constexpr uint32_t kGr16      = 3u;
constexpr uint32_t kDiv000    = 0u;
constexpr uint32_t kDiv001    = 1u;
constexpr uint64_t kDivCkioLimitHz = 20000000u;
constexpr uint8_t  kDonPin    = 0x01u;
constexpr uint64_t kTransferBits = 16u * 8u;
constexpr uint32_t kPhysicalAddressMask = 0x1FFFFFFFu;
constexpr uint8_t  kMstp7     = 0x10u;
constexpr uint8_t  kMstp6     = 0x08u;

constexpr uint16_t kDrMask[7]  = {0x0001u, 0x07FFu, 0xFFFFu, 0x0FFFu, 0x03FFu, 0x03FFu, 0xFBFFu};
constexpr uint16_t kDmrMask[5] = {0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xBFFFu};

constexpr uint64_t kDivMul[5] = {2u, 1u, 1u, 1u, 1u};
constexpr uint64_t kDivDen[5] = {1u, 1u, 2u, 4u, 8u};
constexpr uint32_t kGrBits[4] = {0u, 1u, 2u, 4u};

uint32_t Div(uint16_t ldr1) { return (ldr1 >> 8) & 7u; }
uint32_t Gr(uint16_t ldr1) { return (ldr1 >> 3) & 3u; }
uint32_t Lm(uint16_t ldr1) { return ldr1 & 7u; }
uint32_t BitsPerDot(uint16_t ldr1) { return kGrBits[Gr(ldr1)]; }
uint32_t Nhd(uint16_t ldr2) { return uint32_t{ldr2} >> 8; }
uint32_t Nht(uint16_t ldr2) { return uint32_t{ldr2} & 0xFFu; }
uint32_t Dots(uint16_t ldr2) { return (Nhd(ldr2) + 1u) * 4u; }
uint32_t TotalDots(uint16_t ldr2) { return (Nht(ldr2) + 1u) * 4u; }
uint32_t Lines(uint16_t ldr5) { return (ldr5 & kLineMask) + 1u; }
uint32_t StartPa(uint16_t low, uint16_t high) { return ((uint32_t{high} << 16) | low) & kPhysicalAddressMask; }

}

bool Sh7707Lcdc::ShouldRegister() { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }

void Sh7707Lcdc::OnReady() {
    cpg_    = &emu_.Get<Sh77xxCpg>();
    intc_   = &emu_.Get<Sh77xxIntc>();
    stbcr2_ = &emu_.Get<Sh77xxStbcr2>();
    panel_  = &emu_.Get<Sh7707LcdPanel>();
    mem_    = &emu_.Get<EmulatedMemory>();
    sleep_  = &emu_.Get<GuestDeepSleep>();
    ports_  = &emu_.Get<Sh77xxPortData>();
    emu_.Get<PeripheralDispatcher>().Register(this);
    AttachScanClock();
    stbcr2_->RegisterChangeListener([this](uint8_t, uint8_t) { OnModuleStopChange(); });
    panel_->AddInputListener([this] { Publish(); });
    sleep_->RegisterSleepListener([this](bool asleep) {
        {
            std::lock_guard<std::mutex> lk(state_mtx_);
            standby_ = asleep;
        }
        Publish();
    });
    GuestCpuReset& reset = emu_.Get<GuestCpuReset>();
    reset.RegisterResetListener([this](ResetLineKind kind) { ResetRegisters(emu_.Get<GuestCpuReset>().PowerOnReset(kind)); });
    reset.RegisterResetReleaseListener([this] { OnModuleStopChange(); });
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        display_stopped_  = stbcr2_->ModuleStopped(kMstp7);
        transfer_stopped_ = stbcr2_->ModuleStopped(kMstp6);
    }
}

void Sh7707Lcdc::ResetRegisters(bool power_on) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    StopScanLocked();
    dr_[0] = 0u;
    vi_    = false;
    if (!power_on) return;
    ar_ = 0u;
    dr_.fill(0u);
    pr_.fill(0u);
    dmr_.fill(0u);
    vie_ = false;
}

uint16_t Sh7707Lcdc::ReadHalf(uint32_t addr) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    const uint32_t index = ar_ & kArIndex;
    switch (addr - MmioBase()) {
        case kOffAr: return ar_;
        case kOffDr: return ReadDisplayLocked(addr, index);
        case kOffPr: return pr_[index];
        case kOffDmr:
            if (index >= kDmaRegs) HaltUnsupportedAccess("SH7707 LCDC read of nonexistent LCDDMR", addr, index);
            return dmr_[index];
        default: HaltUnsupportedAccess("SH7707 LCDC ReadHalf", addr, 0);
    }
}

uint16_t Sh7707Lcdc::ReadDisplayLocked(uint32_t addr, uint32_t index) const {
    if (index < kDisplayRegs) return dr_[index];
    if (index == kDr13) return vie_ ? kDr13Vie : 0u;
    HaltUnsupportedAccess("SH7707 LCDC read of nonexistent LCDDR", addr, index);
}

void Sh7707Lcdc::WriteHalf(uint32_t addr, uint16_t value) {
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        const uint32_t index = ar_ & kArIndex;
        switch (addr - MmioBase()) {
            case kOffAr: ar_ = value & kArMask; return;
            case kOffDr:
                WriteDisplayLocked(addr, index, value);
                CheckFetchLocked(addr, value);
                break;
            case kOffPr: pr_[index] = value & kPrMask; return;
            case kOffDmr:
                if (index >= kDmaRegs) HaltUnsupportedAccess("SH7707 LCDC write of nonexistent LCDDMR", addr, value);
                dmr_[index] = value & kDmrMask[index];
                CheckFetchLocked(addr, value);
                return;
            default: HaltUnsupportedAccess("SH7707 LCDC WriteHalf", addr, value);
        }
    }
    ScanEdgesRan();
    Publish();
}

void Sh7707Lcdc::WriteDisplayLocked(uint32_t addr, uint32_t index, uint16_t value) {
    if (index == kDr13) {
        CatchUpLocked(ScanNow());
        vie_ = (value & kDr13Vie) != 0u;
        vi_  = false;
        RearmScanLocked();
        return;
    }
    if (index >= kDisplayRegs) HaltUnsupportedAccess("SH7707 LCDC write of nonexistent LCDDR", addr, value);
    const uint16_t stored = value & kDrMask[index];
    if (index == 1u) CheckModeLocked(addr, stored);
    const bool timing = index == 1u || index == 2u || index == 4u || index == 5u;
    dr_[index]        = stored;
    if (timing) SyncScanLocked(true);
}

void Sh7707Lcdc::CheckModeLocked(uint32_t addr, uint16_t ldr1) const {
    if (Div(ldr1) >= 5u) HaltUnsupportedAccess("SH7707 LCDC LCDDR1 illegal DIV", addr, ldr1);
    if (((ldr1 >> 6) & 3u) == 3u) HaltUnsupportedAccess("SH7707 LCDC LCDDR1 illegal CC", addr, ldr1);
    const uint32_t lm = Lm(ldr1);
    if (lm == 3u || lm >= 6u) HaltUnsupportedAccess("SH7707 LCDC LCDDR1 illegal LM", addr, ldr1);
    if ((ldr1 & kDr1Ren) != 0u && Gr(ldr1) == 0u) {
        HaltUnsupportedAccess("SH7707 LCDC LCDDR1 REN with illegal GR 00", addr, ldr1);
    }
}

void Sh7707Lcdc::CheckFetchLocked(uint32_t addr, uint16_t value) const {
    if ((dr_[1] & kDr1Ren) == 0u) return;
    const bool     dual        = Lm(dr_[1]) == kLmDual;
    const uint64_t panel_bits  = uint64_t{Dots(dr_[2])} * Lines(dr_[5]) * BitsPerDot(dr_[1]);
    const uint64_t frame_bits  = dual ? panel_bits * 2u : panel_bits;
    const uint64_t fetch_bits  = (static_cast<uint64_t>(dmr_[4] & kDmr4Tm) + 1u) * kTransferBits;
    if (frame_bits != fetch_bits) HaltUnsupportedAccess("SH7707 LCDC transfer count does not match the display", addr, value);
    CheckPanelDataLocked(addr, value, dmr_[0], dmr_[1], panel_bits / 8u);
    if (dual) CheckPanelDataLocked(addr, value, dmr_[2], dmr_[3], panel_bits / 8u);
    CheckTimingLocked(addr, value);
}

void Sh7707Lcdc::CheckTimingLocked(uint32_t addr, uint16_t value) const {
    const uint32_t              lm         = Lm(dr_[1]);
    const uint32_t              div        = Div(dr_[1]);
    const bool                  color_mode = lm == kLmTft || lm == kLmStnColor;
    const bool                  mono_stn   = lm == kLmDual || ((lm == 0u || lm == 1u) && !panel_->ShowsColor());
    const GuestCycleClock::Rate ckio       = cpg_->InternalCkioRate();
    const bool                  fast_ckio  = ckio.num > kDivCkioLimitHz * ckio.den;
    if (div == kDiv000 && (color_mode || Gr(dr_[1]) == kGr16)) {
        HaltUnsupportedAccess("SH7707 LCDC DIV 000 in a 16-gradation or color mode", addr, value);
    }
    if (div == kDiv000 && mono_stn && fast_ckio) {
        HaltUnsupportedAccess("SH7707 LCDC DIV 000 in a 2- or 4-gradation STN mode above CKIO 20 MHz", addr, value);
    }
    if (div == kDiv001 && lm == kLmTft && fast_ckio) {
        HaltUnsupportedAccess("SH7707 LCDC DIV 001 in mode G above CKIO 20 MHz", addr, value);
    }
    if (Lines(dr_[5]) > Lines(dr_[4])) HaltUnsupportedAccess("SH7707 LCDC Nvd above Nvt", addr, value);
    if (Nhd(dr_[2]) + 2u > Nht(dr_[2])) HaltUnsupportedAccess("SH7707 LCDC Nhd + 2 above Nht", addr, value);
}

void Sh7707Lcdc::CheckPanelDataLocked(uint32_t addr, uint16_t value, uint16_t low, uint16_t high, uint64_t bytes) const {
    if ((low & 3u) != 0u) HaltUnsupportedAccess("SH7707 LCDC display start address not longword aligned", addr, value);
    if (mem_->TryTranslateRange(StartPa(low, high), bytes) == nullptr) {
        HaltUnsupportedAccess("SH7707 LCDC display data outside backed memory", addr, value);
    }
}

bool Sh7707Lcdc::DonPinLocked() const { return (dr_[0] & kDr0Don) != 0u && !display_stopped_ && !standby_; }

void Sh7707Lcdc::DriveDonPin() {
    bool low;
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        low = !DonPinLocked();
    }
    ports_->SetFunctionOutputs(kSh77xxPortD, kDonPin, low ? PinDrive::kLow : PinDrive::kFunction);
}

bool Sh7707Lcdc::ScanWantedLocked() const { return !display_stopped_ && Lines(dr_[5]) < Lines(dr_[4]); }

void Sh7707Lcdc::SyncScanLocked(bool timing_changed) {
    const uint64_t now = ScanNow();
    if (ScanLiveLocked()) {
        CatchUpLocked(now);
        if (timing_changed || !ScanWantedLocked()) StopScanLocked();
    }
    if (ScanWantedLocked() && !ScanLiveLocked()) StartScanLocked(now);
}

RasterScanPeripheral::ScanShape Sh7707Lcdc::ScanShapeLocked() const {
    const uint32_t              div       = Div(dr_[1]);
    const uint64_t              line_dots = TotalDots(dr_[2]);
    const GuestCycleClock::Rate ckio      = cpg_->InternalCkioRate();
    RasterScanClock::Frame      frame;
    frame.ticks   = Lines(dr_[4]);
    frame.edge[0] = Lines(dr_[5]);
    frame.edges   = 1u;
    return ScanShape{{ckio.num * kDivMul[div], ckio.den * kDivDen[div] * line_dots}, frame};
}

void Sh7707Lcdc::FrameEdgeLocked(uint32_t) { vi_ = true; }

void Sh7707Lcdc::ScanEdgesRan() {
    bool level;
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        level = vi_ && vie_;
    }
    intc_->SetSourceRequest(Sh77xxIntcSource::kLcdi, level);
}

void Sh7707Lcdc::OnModuleStopChange() {
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        display_stopped_  = stbcr2_->ModuleStopped(kMstp7);
        transfer_stopped_ = stbcr2_->ModuleStopped(kMstp6);
        SyncScanLocked(false);
    }
    ScanEdgesRan();
    Publish();
}

void Sh7707Lcdc::Publish() {
    DriveDonPin();
    bool stopped;
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        stopped = display_stopped_;
    }
    const bool lit = panel_->Lit();
    if (lit && stopped) emu_.Get<Fatal>().Die("Sh7707Lcdc: display stop (STBCR2 MSTP7) with the board panel lit");
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        lit_ = lit;
    }
    const Scanout s = GetScanout();
    latch_.Publish(emu_, lit, s.dots, s.PresentedLines());
}

Sh7707Lcdc::Scanout Sh7707Lcdc::GetScanout() const {
    std::lock_guard<std::mutex> lk(state_mtx_);
    Scanout s;
    s.panel_lit    = lit_;
    s.data_out     = (dr_[1] & kDr1Ren) != 0u && !transfer_stopped_;
    s.linear_bytes = (dmr_[4] & kDmr4Lem) != 0u;
    s.mode         = Lm(dr_[1]);
    s.dual         = s.mode == kLmDual && panel_->TakesLowerPanelData();
    s.bits_per_dot = BitsPerDot(dr_[1]);
    s.dots         = Dots(dr_[2]);
    s.panel_lines  = Lines(dr_[5]);
    s.upper_pa     = StartPa(dmr_[0], dmr_[1]);
    s.lower_pa     = StartPa(dmr_[2], dmr_[3]);
    s.palette      = pr_;
    return s;
}

void Sh7707Lcdc::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    w.Write("lcdar", ar_);
    w.WriteBytes("lcddr", dr_.data(), sizeof(dr_));
    w.Write<uint8_t>("vie", vie_ ? 1u : 0u);
    w.Write<uint8_t>("vi", vi_ ? 1u : 0u);
    w.WriteBytes("lcdpr", pr_.data(), sizeof(pr_));
    w.WriteBytes("lcddmr", dmr_.data(), sizeof(dmr_));
    latch_.SaveState(w);
    SaveScanLocked(w);
}

void Sh7707Lcdc::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    r.Read("lcdar", ar_);
    r.ReadBytes("lcddr", dr_.data(), sizeof(dr_));
    uint8_t flag = 0u;
    r.Read("vie", flag);
    vie_ = flag != 0u;
    r.Read("vi", flag);
    vi_ = flag != 0u;
    r.ReadBytes("lcdpr", pr_.data(), sizeof(pr_));
    r.ReadBytes("lcddmr", dmr_.data(), sizeof(dmr_));
    latch_.RestoreState(r);
    RestoreScanLocked(r);
}

void Sh7707Lcdc::PostRestore() {
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        display_stopped_  = stbcr2_->ModuleStopped(kMstp7);
        transfer_stopped_ = stbcr2_->ModuleStopped(kMstp6);
        standby_          = sleep_->Asleep();
        if (ScanWantedLocked()) ResumeScanLocked(ScanNow());
    }
    ScanEdgesRan();
    Publish();
}

REGISTER_SERVICE(Sh7707Lcdc);

}

#include "../vr41xx/vr41xx_kiu.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>
#include "../../cpu/mips/mips_core_clock.h"
#include "../vr41xx/vr41xx_dma_page_interrupts.h"
#include "../vr41xx/vr41xx_kiu_regs.h"
#include "vr4101_dma_addresses.h"
#include "vr4101_dma_control.h"
#include "vr4101_id.h"
#include "vr4101_interval_unit.h"

#include <cstdint>

namespace cerfcore {

namespace {

using namespace cerf_vr41xx_kiu_regs;

constexpr uint32_t kOffDat      = 0x00u;
constexpr uint32_t kOffAScan    = 0x04u;
constexpr uint32_t kOffAStop    = 0x06u;
constexpr uint32_t kOffScan     = 0x08u;
constexpr uint32_t kOffStop     = 0x0Au;
constexpr uint32_t kOffSap      = 0x0Cu;
constexpr uint32_t kOffScans    = 0x0Eu;
constexpr uint32_t kOffWks      = 0x10u;
constexpr uint32_t kOffWki      = 0x12u;
constexpr uint32_t kOffSrn      = 0x14u;
constexpr uint32_t kOffInt      = 0x16u;
constexpr uint32_t kOffRst      = 0x18u;
constexpr uint32_t kOffEn       = 0x1Au;
constexpr uint32_t kOffDozeKey  = 0x1Cu;
constexpr uint32_t kOffEvVol    = 0x1Eu;

constexpr uint16_t kFlagBit      = 0x0001u;
constexpr uint16_t kKeyPage      = 1u << 3;
constexpr uint16_t kKeyEnd       = 1u << 4;
constexpr uint16_t kPageCauses   = kKeyPage | kKeyEnd;
constexpr Vr41xxDmaPageInterrupts kKeyPages{kKeyPage, kKeyEnd};
constexpr uint16_t kIntBits      = kScanInt | kKDatRdy | kKDatLost | kKeyPage | kKeyEnd;
constexpr uint16_t kEvVolBits    = 0x0005u;
constexpr uint16_t kScanLine8    = 0x0002u;
constexpr uint32_t kScanWords    = 4u;

constexpr Vr41xxKiuModel kModel = {
    .stprep_zero_count        = 64u,
    .scanstart_auto_clear     = false,
    .keyen_scanline_interlock = false,
    .keyen_stop_deferred      = false,
    .scanstart_while_running_defined = false,
    .scanstp_completes_when_idle     = true,
    .gpen_retained_on_other_reset = false,
    .gpen_survives_kiurst         = false,
    .scanline_none_drives_all     = false,
    .scan_pins_drive_piod         = false,
    .count_bias                   = 0u,
    .zero_count_rfu               = false,
};

class Vr4101Kiu : public Vr41xxKiu {
public:
    using Vr41xxKiu::Vr41xxKiu;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4101; }

    uint32_t ReadWord(uint32_t addr) override { return ReadWordAsHalves(addr); }

    void WriteWord(uint32_t addr, uint32_t value) override { WriteWordAsHalves(addr, value); }

    uint16_t ReadHalf(uint32_t addr) override {
        std::lock_guard<std::mutex> lk(mtx_);
        BeginReadLocked(addr);
        const uint16_t rep = seq_.ScanRep();
        switch (addr - MmioBase()) {
            case kOffDat:     return last_word_;
            case kOffAScan:   return Flag(rep, kAtScan);
            case kOffAStop:   return Flag(rep, kAtStp);
            case kOffScan:    return Flag(rep, kScanStart);
            case kOffStop:    return Flag(rep, kScanStp);
            case kOffSap:     return sap_;
            case kOffScans:   return seq_.State();
            case kOffWks:     return wks_;
            case kOffWki:     return wintvl_;
            case kOffSrn:     return static_cast<uint16_t>((rep >> kStpRepShift) & kStpRepBits);
            case kOffInt:     return causes_;
            case kOffRst:     return 0u;
            case kOffEn:      return Flag(rep, kKeyen);
            case kOffEvVol:   return evvol_;
            case kOffDozeKey: HaltDozeKey(addr, 0u);
            default:          HaltUnsupportedAccess("VR4101 KIU ReadHalf", addr, 0);
        }
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t off = addr - MmioBase();
        std::lock_guard<std::mutex> lk(mtx_);
        const bool scan_running = BeginWriteLocked(addr, value, off == kOffRst);
        switch (off) {
            case kOffAScan: WriteFlagLocked(addr, value, kAtScan);    return;
            case kOffAStop: WriteFlagLocked(addr, value, kAtStp);     return;
            case kOffScan:  WriteFlagLocked(addr, value, kScanStart); return;
            case kOffStop:  WriteFlagLocked(addr, value, kScanStp);   return;
            case kOffEn:    WriteFlagLocked(addr, value, kKeyen);     return;
            case kOffSrn:
                RequireBits(addr, value, kStpRepBits);
                WriteScanRepLocked(addr, value, static_cast<uint16_t>(value << kStpRepShift),
                                   static_cast<uint16_t>(kStpRepBits << kStpRepShift));
                return;
            case kOffSap:
                RequireBits(addr, value, kFlagBit);
                sap_ = value;
                return;
            case kOffInt:
                RequireBits(addr, value, kIntBits);
                ClearCausesLocked(value);
                return;
            case kOffRst:
                RequireBits(addr, value, kKiuRst);
                if (value & kKiuRst) KiuResetLocked();
                else                 UpdateLocked();
                return;
            case kOffWki:
                RequireBits(addr, value, kWintvlBits);
                WriteWkiLocked(value);
                return;
            case kOffWks:
                RequireBits(addr, value, kWksMask);
                WriteWksLocked(value, scan_running);
                return;
            case kOffEvVol:
                RequireBits(addr, value, kEvVolBits);
                evvol_ = value;
                return;
            case kOffDozeKey: HaltDozeKey(addr, value);
            default:          HaltUnsupportedAccess("VR4101 KIU WriteHalf", addr, value);
        }
    }

protected:
    const Vr41xxKiuModel& Model() const override { return kModel; }

    void AttachFrontEnd() override {
        dcu_ = &emu_.Get<Vr4101DmaControl>();
        dma_ = &emu_.Get<Vr4101DmaAddresses>();
        std::lock_guard<std::mutex> lk(mtx_);
        scanline_ = kScanLine8;
    }

    GuestCycleClock::Rate ScanTickRate() override {
        auto& core = emu_.Get<MipsCoreClock>();
        return Vr4101IntervalUnitRate(core.ResetRate(), core.CyclesPerTclkCounterTick());
    }

    uint16_t PiodLocked() const override { return 0u; }

    void ScanStoredLocked() override {
        for (uint32_t i = 0; i < kScanWords; ++i) {
            last_word_ = key_matrix_.Data(i);
            if (!DmaRequestLocked()) {
                causes_ |= kKDatLost;
                return;
            }
            if (dma_->Store(Vr4101DmaChannel::kKiu, last_word_)) causes_ |= kKeyPages.AtBoundary(causes_);
        }
        causes_ |= kKDatRdy;
    }

    void ResetFrontEndLocked() override {
        scanline_  = kScanLine8;
        evvol_     = 0u;
        sap_       = 0u;
        last_word_ = 0u;
    }

    void SaveFrontEnd(StateWriter& w) override {
        w.Write("evvol", evvol_);
        w.Write("sap", sap_);
        w.Write("last_word", last_word_);
    }

    void RestoreFrontEnd(StateReader& r) override {
        r.Read("evvol", evvol_);
        r.Read("sap", sap_);
        r.Read("last_word", last_word_);
    }

private:
    static uint16_t Flag(uint16_t rep, uint16_t bit) { return (rep & bit) != 0u ? kFlagBit : 0u; }

    bool DmaRequestLocked() const {
        if (!dcu_->TransferEnabled(Vr4101DmaChannel::kKiu)) return false;
        return (sap_ & kFlagBit) == 0u || (causes_ & kPageCauses) == 0u;
    }

    void RequireBits(uint32_t addr, uint16_t value, uint16_t allowed) {
        if ((value & ~allowed) != 0u) HaltUnsupportedAccess("VR4101 KIU reserved bit", addr, value);
    }

    [[noreturn]] void HaltDozeKey(uint32_t addr, uint16_t value) {
        emu_.Get<Fatal>().Die("VR4101 KIU DOZEKEYINTREG access at 0x%08X (0x%04X): the Suspend-mode key "
                              "interrupt register", addr, value);
    }

    void WriteFlagLocked(uint32_t addr, uint16_t value, uint16_t scanrep_bit) {
        RequireBits(addr, value, kFlagBit);
        WriteScanRepLocked(addr, value, value != 0u ? scanrep_bit : uint16_t{0}, scanrep_bit);
    }

    Vr4101DmaControl*   dcu_       = nullptr;
    Vr4101DmaAddresses* dma_       = nullptr;
    uint16_t            evvol_     = 0u;
    uint16_t            sap_       = 0u;
    uint16_t            last_word_ = 0u;
};

}

REGISTER_SERVICE_AS(Vr4101Kiu, Vr41xxKiu);

}

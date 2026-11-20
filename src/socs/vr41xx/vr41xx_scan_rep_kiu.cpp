#include "vr41xx_scan_rep_kiu.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfengine/host_request_channel.h>
#include "vr41xx_kiu_regs.h"

namespace cerfcore {

using namespace cerf_vr41xx_kiu_regs;

void Vr41xxScanRepKiu::AttachFrontEnd() {
    giu_ = &emu_.Get<Vr41xxPortGiu>();
    giu_->RegisterOutputDataLowListener([this](uint16_t previous) {
        std::lock_guard<std::mutex> lk(mtx_);
        AdvancePhaseLocked(static_cast<uint16_t>(previous & kGpenMask));
        seq_.ReevaluateWaitKeyIn();
        PublishCausesLocked();
        UpdateLocked();
    });
    giu_->RegisterAlternateFunction(kFirstKscanGpio, kFirstKscanGpio + kScanLines - 1,
                                    [this](int gpio) {
                                        std::lock_guard<std::mutex> lk(mtx_);
                                        return KscanDriveLocked(gpio);
                                    });
}

GuestCycleClock::Rate Vr41xxScanRepKiu::ScanTickRate() {
    return GuestCycleClock::Rate{Vr41xxRtcxTicks::kHz, 1u};
}

uint16_t Vr41xxScanRepKiu::PiodLocked() const {
    return static_cast<uint16_t>(giu_->OutputDataLow() & kGpenMask);
}

void Vr41xxScanRepKiu::ScanStoredLocked() {
    data_unread_ = true;
    causes_ |= kKDatRdy;
}

Vr41xxScanRepKiu::KscanDrive Vr41xxScanRepKiu::KscanDriveMasksLocked() const {
    KscanDrive d;
    d.released = key_matrix_.OutputPins(scanline_, gpen_);
    if (hibernating_) return d;
    const uint16_t scan = static_cast<uint16_t>(key_matrix_.ScanPins(scanline_) & ~d.released);
    if (undefined_) {
        d.undefined = scan;
        return d;
    }
    if (seq_.State() != kSStatWaitKeyIn) return d;
    d.driven = scan;
    d.high   = static_cast<uint16_t>(scan & key_matrix_.ScanLevel(PiodLocked()));
    return d;
}

std::optional<PinDrive> Vr41xxScanRepKiu::KscanDriveLocked(int gpio) {
    const uint16_t   bit = static_cast<uint16_t>(1u << (gpio - kFirstKscanGpio));
    const KscanDrive d   = KscanDriveMasksLocked();
    if (d.released & bit) return std::nullopt;
    if (d.undefined & bit)
        HaltUnsupportedAccess("KIU KSCAN pin level while the sequencer state is undefined",
                              kBase + kOffScanLine, scanline_);
    if ((d.driven & bit) == 0u) return PinDrive::kUndriven;
    return (d.high & bit) ? PinDrive::kHigh : PinDrive::kLow;
}

uint64_t Vr41xxScanRepKiu::DriveSignatureLocked() const {
    const KscanDrive d = KscanDriveMasksLocked();
    return uint64_t{d.released} | (uint64_t{d.undefined} << 16) | (uint64_t{d.driven} << 32) |
           (uint64_t{d.high} << 48);
}

void Vr41xxScanRepKiu::PinsChangedLocked() {
    const uint64_t signature = DriveSignatureLocked();
    if (signature == drive_signature_) return;
    drive_signature_ = signature;
    drive_changed_.store(true, std::memory_order_release);
    host_requests_->Request();
}

void Vr41xxScanRepKiu::FlushPinsChange() {
    if (drive_changed_.exchange(false, std::memory_order_acq_rel)) giu_->NotifyOutputDriveChanged();
}

uint16_t Vr41xxScanRepKiu::ReadHalf(uint32_t addr) {
    const uint32_t off = addr - kBase;
    std::lock_guard<std::mutex> lk(mtx_);
    BeginReadLocked(addr);
    if (off <= kOffDat5 && (off & 1u) == 0u) {
        data_unread_ = false;
        ArmEventLocked();
        return key_matrix_.Data(off >> 1);
    }
    switch (off) {
        case kOffScanRep:  return seq_.ScanRep();
        case kOffScans:    return seq_.State();
        case kOffWks:      return wks_;
        case kOffWki:      return wintvl_;
        case kOffInt:      return causes_;
        case kOffScanLine: return scanline_;
        case kOffGpen:     return gpen_;
        default:           HaltUnsupportedAccess("KIU ReadHalf", addr, 0);
    }
}

void Vr41xxScanRepKiu::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t off = addr - kBase;
    std::lock_guard<std::mutex> lk(mtx_);
    const bool scan_running = BeginWriteLocked(addr, value, off == kOffRst);
    switch (off) {
        case kOffScanRep:
            WriteScanRepLocked(addr, value, value, kScanRepMask);
            return;
        case kOffInt:
            ClearCausesLocked(static_cast<uint16_t>(value & kIntMask));
            return;
        case kOffRst:
            if (value & kKiuRst) KiuResetLocked();
            else                 UpdateLocked();
            return;
        case kOffWki:
            WriteWkiLocked(static_cast<uint16_t>(value & kWintvlBits));
            return;
        case kOffWks:
            WriteWksLocked(static_cast<uint16_t>(value & kWksMask), scan_running);
            return;
        case kOffScanLine: {
            const uint16_t v = static_cast<uint16_t>(value & kScanLineMask);
            const bool reconfigures = v != scanline_ &&
                                      (scan_running || (seq_.Enabled() && ScanLineCount(v) == 0u));
            scanline_ = v;
            if (reconfigures) {
                EnterUndefinedLocked();
                return;
            }
            seq_.ReevaluateWaitKeyIn();
            PublishCausesLocked();
            UpdateLocked();
            return;
        }
        case kOffGpen:
            gpen_ = value & kGpenMask;
            seq_.ReevaluateWaitKeyIn();
            PublishCausesLocked();
            UpdateLocked();
            return;
        default:           HaltUnsupportedAccess("KIU WriteHalf", addr, value);
    }
}

}

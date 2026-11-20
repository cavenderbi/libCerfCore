#pragma once

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../socs/reg_window_impl.h"
#include "hitachi_pcc.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

namespace hitachi_pcc_detail {

using cerf_reg_window_detail::OtherReset;
using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegSpec;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

constexpr RegSpec InterfaceStatus() {
    return RegSpec{ReadKind::kSampled, WriteKind::kFatal, 0x00u, 0x00u, 0x00u, OtherReset::kRetain,
                   0x00u, 0x00u, true};
}

constexpr RegSpec Control(uint16_t writable, uint16_t fatal_on_set) {
    return RegSpec{ReadKind::kStored, WriteKind::kStored, writable, 0x00u, fatal_on_set, OtherReset::kRetain,
                   0x00u, 0x00u, true};
}

constexpr RegSpec ChangeStatus(uint16_t writable, uint16_t reserved) {
    return RegSpec{ReadKind::kSampled, WriteKind::kHooked, writable, 0x00u, reserved, OtherReset::kRetain,
                   0x00u, 0x00u, true};
}

inline constexpr uint32_t kChannels    = 2u;
inline constexpr uint32_t kChannelRegs = 8u;
inline constexpr uint32_t kIsr         = 0u;
inline constexpr uint32_t kGcr         = 1u;
inline constexpr uint32_t kCscr        = 2u;
inline constexpr uint32_t kCscier      = 3u;
inline constexpr uint32_t kArea6       = 6u;

inline constexpr uint16_t kIsrReady        = 0x80u;
inline constexpr uint16_t kIsrWriteProtect = 0x40u;
inline constexpr uint16_t kIsrVs2          = 0x20u;
inline constexpr uint16_t kIsrVs1          = 0x10u;
inline constexpr uint16_t kIsrCd2          = 0x08u;
inline constexpr uint16_t kIsrCd1          = 0x04u;
inline constexpr uint16_t kIsrBvd2         = 0x02u;
inline constexpr uint16_t kIsrBvd1         = 0x01u;

inline constexpr uint16_t kGcrDriveEnable  = 0x80u;
inline constexpr uint16_t kGcrCardReset    = 0x40u;
inline constexpr uint16_t kGcrIoInterface  = 0x20u;
inline constexpr uint16_t kGcr16MbAreaMode = 0x08u;
inline constexpr uint16_t kGcrPa25         = 0x04u;
inline constexpr uint16_t kGcrPa24         = 0x02u;
inline constexpr uint16_t kGcrReg          = 0x01u;

inline constexpr uint16_t kCscrSoftwareDetect = 0x80u;
inline constexpr uint16_t kCscrIreq           = 0x20u;
inline constexpr uint16_t kCscrStatusChange   = 0x10u;
inline constexpr uint16_t kCscrDetectChange   = 0x08u;
inline constexpr uint16_t kCscrReadyChange    = 0x04u;
inline constexpr uint16_t kCscrBatteryWarning = 0x02u;
inline constexpr uint16_t kCscrBatteryDead    = 0x01u;
inline constexpr uint16_t kCscrWriteZeroClear = 0x1Fu;

inline constexpr uint16_t kCscierSourceEnables = 0x1Fu;
inline constexpr uint16_t kCscierDetectEnable  = 0x08u;
inline constexpr uint16_t kCscierIreqModeMask  = 0x60u;
inline constexpr uint32_t kCscierIreqModeShift = 5u;

inline constexpr uint32_t kIreqNone        = 0u;
inline constexpr uint32_t kIreqFallingEdge = 2u;
inline constexpr uint32_t kIreqRisingEdge  = 3u;

inline constexpr uint32_t kBlockShift     = 24u;
inline constexpr uint32_t kBlockMask      = 0x00FFFFFFu;
inline constexpr uint32_t kAttributeBlock = 0u;
inline constexpr uint32_t kIoBlock        = 2u;
inline constexpr uint32_t kPaShift        = 23u;
inline constexpr uint32_t kWindow32Mask   = 0x01FFFFFFu;
inline constexpr uint32_t kCardA25        = 0x02000000u;

template <const RegWindowModel& Model, bool kPinResetOnly>
class HitachiPccBase : public cerf_reg_window_detail::RegWindow<Model, Peripheral, std::mutex, kPinResetOnly>,
                       public HitachiPcc {
    using Window = cerf_reg_window_detail::RegWindow<Model, Peripheral, std::mutex, kPinResetOnly>;

public:
    explicit HitachiPccBase(CerfEmulator& emu)
        : Window(emu), slot0_(emu, *this, ObjectIdentity{}), slot1_(emu, *this, ObjectIdentity{}),
          slots_{&slot0_, &slot1_} {}

    void OnReady() override {
        auto& tree = this->emu_.template Get<MachineObjectTree>();
        for (uint32_t ch = 0; ch < kChannels; ++ch) {
            if (!SocketFitted(ch)) continue;
            slots_[ch]->SetIdentity(SocketIdentity(ch));
            tree.Add(slots_[ch]);
        }
        Window::OnReady();
        ApplySocketDrive();
        {
            std::lock_guard<std::mutex> lk(this->lock_);
            for (uint32_t ch = 0; ch < kChannels; ++ch) card_detect_seen_[ch] = slots_[ch]->HasCard();
        }
        auto& reset = this->emu_.template Get<GuestCpuReset>();
        reset.RegisterResetListener([this](ResetLineKind kind) {
            if (!this->IsPowerOnReset(kind)) return;
            std::lock_guard<std::mutex> lk(this->lock_);
            for (uint32_t ch = 0; ch < kChannels; ++ch) {
                ready_pin_high_[ch]   = ReadyPinHighLocked(ch);
                card_detect_seen_[ch] = slots_[ch]->HasCard();
            }
        });
        reset.RegisterResetReleaseListener([this] {
            ApplySocketDrive();
            std::lock_guard<std::mutex> lk(this->lock_);
            for (uint32_t ch = 0; ch < kChannels; ++ch) RefreshChannelLocked(ch);
        });
    }

    void OnShutdown() override {
        slot0_.OnShutdown();
        slot1_.OnShutdown();
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        Window::WriteByte(addr, value);
        ApplySocketDrive();
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        Window::WriteHalf(addr, value);
        ApplySocketDrive();
    }

    HitachiPccSocketPins Pins(uint32_t ch) {
        std::lock_guard<std::mutex> lk(this->lock_);
        return PinsLocked(ch);
    }

    HitachiPccCardCycle DecodeCycle(uint32_t area, uint32_t offset, bool io, bool write) override {
        const uint32_t ch   = area == kArea6 ? 0u : 1u;
        const uint32_t base = ch * kChannelRegs;
        RequireCardCycle(ch, offset);
        std::lock_guard<std::mutex> lk(this->lock_);
        const uint16_t      gcr        = this->StoredReg(base + kGcr);
        const bool          sixteen_mb = (gcr & kGcr16MbAreaMode) != 0u;
        const bool          protect    = write && WriteProtectedLocked(ch);
        HitachiPccCardCycle c{slots_[ch], 0u, false, false};
        if (io) {
            if (ch != 0u) {
                this->emu_.template Get<Fatal>().Die("%s: I/O cycle at offset 0x%08X to PCC1, which has the IC memory "
                                                     "card interface only", typeid(*this).name(), offset);
            }
            if (sixteen_mb && (offset >> kBlockShift) != kIoBlock) {
                this->emu_.template Get<Fatal>().Die("%s: I/O cycle at offset 0x%08X in the unused 16 MB block of PCC%u",
                                                     typeid(*this).name(), offset, ch);
            }
            if (protect) {
                this->emu_.template Get<Fatal>().Die("%s: I/O write at offset 0x%08X to PCC%u with write protect set",
                                                     typeid(*this).name(), offset, ch);
            }
            c.card_address = offset & (sixteen_mb ? kBlockMask : kWindow32Mask);
            return c;
        }
        if (sixteen_mb) {
            c.attribute    = (offset >> kBlockShift) == kAttributeBlock;
            c.card_address = offset & kBlockMask;
            if (!c.attribute) c.card_address |= static_cast<uint32_t>(gcr & (kGcrPa25 | kGcrPa24)) << kPaShift;
        } else {
            c.attribute    = (gcr & kGcrReg) == 0u;
            c.card_address = offset & kWindow32Mask;
            if (!c.attribute && (gcr & kGcrPa25) != 0u) c.card_address |= kCardA25;
        }
        c.dropped = protect;
        return c;
    }

    void OnCardDetectChanged(PcmciaSlot& slot) override {
        std::lock_guard<std::mutex> lk(this->lock_);
        RefreshChannelLocked(ChannelOf(slot));
    }

    void OnReadyIreqLow(PcmciaSlot& slot) override { SetReadyIreqLow(slot, true); }
    void OnReadyIreqHigh(PcmciaSlot& slot) override { SetReadyIreqLow(slot, false); }

    void OnStatusChange(PcmciaSlot& slot, bool low) override {
        const uint32_t              ch = ChannelOf(slot);
        std::lock_guard<std::mutex> lk(this->lock_);
        if (low == stschg_low_[ch]) return;
        stschg_low_[ch] = low;
        if (low && IoInterfaceLocked(ch)) LatchLocked(ch, kCscrStatusChange);
        UpdateRequestLocked(ch);
    }

    void OnCardPowerChanged(PcmciaSlot& slot) override {
        const uint32_t              ch = ChannelOf(slot);
        std::lock_guard<std::mutex> lk(this->lock_);
        TrackReadyPinLocked(ch);
        UpdateRequestLocked(ch);
    }

    uint8_t UndrivenDataLane() const override { return kUnstatedUndrivenDataLane; }

    void SaveState(StateWriter& w) override {
        Window::SaveState(w);
        {
            std::lock_guard<std::mutex> lk(this->lock_);
            for (uint32_t ch = 0; ch < kChannels; ++ch) {
                w.Write<uint8_t>("ready_ireq_low", ready_ireq_low_[ch] ? 1u : 0u);
                w.Write<uint8_t>("ready_pin_high", ready_pin_high_[ch] ? 1u : 0u);
                w.Write<uint8_t>("card_detect_seen", card_detect_seen_[ch] ? 1u : 0u);
                w.Write<uint8_t>("stschg_low", stschg_low_[ch] ? 1u : 0u);
            }
        }
        for (uint32_t ch = 0; ch < kChannels; ++ch) {
            w.BeginFrame(ch);
            slots_[ch]->SaveSlotState(w);
            w.EndFrame();
        }
    }

    void RestoreState(StateReader& r) override {
        Window::RestoreState(r);
        {
            std::lock_guard<std::mutex> lk(this->lock_);
            for (uint32_t ch = 0; ch < kChannels; ++ch) {
                uint8_t low  = 0u;
                uint8_t high = 0u;
                uint8_t card = 0u;
                r.Read("ready_ireq_low", low);
                r.Read("ready_pin_high", high);
                r.Read("card_detect_seen", card);
                uint8_t stschg = 0u;
                r.Read("stschg_low", stschg);
                ready_ireq_low_[ch]   = low != 0u;
                ready_pin_high_[ch]   = high != 0u;
                card_detect_seen_[ch] = card != 0u;
                stschg_low_[ch]       = stschg != 0u;
            }
        }
        for (uint32_t ch = 0; ch < kChannels; ++ch) {
            if (const uint32_t frame = r.EnterFrame(); frame != ch) {
                r.Reject("PC Card socket frame 0x%X where this build writes socket %u", frame, ch);
            }
            slots_[ch]->RestoreSlotState(r);
            r.LeaveFrame();
        }
    }

    void PostRestore() override {
        ApplySocketDrive();
        for (uint32_t ch = 0; ch < kChannels; ++ch) slots_[ch]->PostRestoreSlot();
        std::lock_guard<std::mutex> lk(this->lock_);
        for (uint32_t ch = 0; ch < kChannels; ++ch) UpdateRequestLocked(ch);
    }

protected:
    virtual ObjectIdentity       SocketIdentity(uint32_t ch) const                                      = 0;
    virtual bool                 SocketFitted(uint32_t ch) const                                        = 0;
    virtual bool                 SocketPoweredLocked(uint32_t ch) const                                 = 0;
    virtual bool                 WriteProtectedLocked(uint32_t ch) const                                = 0;
    virtual HitachiPccSocketPins PresentLocked(uint32_t ch, const HitachiPccSocketPins& live) const    = 0;
    virtual void                 DeliverCausesLocked(uint32_t ch, uint32_t causes)                      = 0;
    virtual void                 RequireCardCycle(uint32_t ch, uint32_t offset)                         = 0;

    HitachiPccSocketPins PinsLocked(uint32_t ch) const {
        const PcmciaSlot& slot    = *slots_[ch];
        const bool        no_card = !slot.HasCard();
        return HitachiPccSocketPins{ReadyPinHighLocked(ch),
                                    !IoInterfaceLocked(ch) && !slot.IsPowered(),
                                    !slot.Vs2Grounded(),
                                    !slot.Vs1Grounded(),
                                    no_card,
                                    no_card,
                                    true,
                                    !(IoInterfaceLocked(ch) && slot.IsPowered() && stschg_low_[ch]),
                                    true};
    }

    bool SlotPowered(uint32_t ch) const { return slots_[ch]->IsPowered(); }

    void RefreshChannelLocked(uint32_t ch) {
        SyncCardDetectLocked(ch);
        TrackReadyPinLocked(ch);
        UpdateRequestLocked(ch);
    }

    void ApplySocketDrive() {
        SocketDrive drive[kChannels];
        {
            std::lock_guard<std::mutex> lk(this->lock_);
            for (uint32_t ch = 0; ch < kChannels; ++ch) drive[ch] = SocketDriveLocked(ch);
        }
        for (uint32_t ch = 0; ch < kChannels; ++ch) {
            if (!SocketFitted(ch)) continue;
            slots_[ch]->SetPowered(drive[ch].powered);
            slots_[ch]->SetResetLine(drive[ch].reset);
            slots_[ch]->SetCardEnabled(drive[ch].enabled);
        }
    }

    uint16_t SampleRegister(uint32_t i, uint32_t addr) override {
        (void)addr;
        const uint32_t ch = i / kChannelRegs;
        if (i % kChannelRegs == kIsr) return InterfaceStatusLocked(ch);
        return ChangeStatusLocked(ch);
    }

    void WriteHooked(uint32_t i, uint32_t addr, uint16_t value) override {
        (void)addr;
        const uint32_t ch     = i / kChannelRegs;
        const uint16_t clears = ch == 0u && IreqEdgeModeLocked() ? static_cast<uint16_t>(kCscrWriteZeroClear | kCscrIreq)
                                                                 : kCscrWriteZeroClear;
        const uint16_t old    = this->StoredReg(i);
        const uint16_t next   = static_cast<uint16_t>((old & ~(clears | kCscrSoftwareDetect)) | (old & value & clears) |
                                                    (value & kCscrSoftwareDetect));
        this->SetStoredReg(i, next);
        UpdateRequestLocked(ch);
    }

    void AfterStore(uint32_t i) override {
        (void)i;
        for (uint32_t ch = 0; ch < kChannels; ++ch) UpdateRequestLocked(ch);
    }

private:
    struct SocketDrive {
        bool powered;
        bool reset;
        bool enabled;
    };

    uint32_t ChannelOf(const PcmciaSlot& slot) const { return &slot == slots_[0] ? 0u : 1u; }

    bool ReadyPinHighLocked(uint32_t ch) const { return !(slots_[ch]->IsPowered() && ready_ireq_low_[ch]); }

    bool IoInterfaceLocked(uint32_t ch) const {
        return ch == 0u && (this->StoredReg(kGcr) & kGcrIoInterface) != 0u;
    }

    uint32_t IreqModeLocked() const {
        return (this->StoredReg(kCscier) & kCscierIreqModeMask) >> kCscierIreqModeShift;
    }

    bool IreqEdgeModeLocked() const {
        const uint32_t mode = IreqModeLocked();
        return mode == kIreqFallingEdge || mode == kIreqRisingEdge;
    }

    uint16_t InterfaceStatusLocked(uint32_t ch) const {
        const HitachiPccSocketPins p = PresentLocked(ch, PinsLocked(ch));
        uint16_t                   v = 0u;
        if (p.ready) v |= kIsrReady;
        if (p.write_protect) v |= kIsrWriteProtect;
        if (p.vs2) v |= kIsrVs2;
        if (p.vs1) v |= kIsrVs1;
        if (p.cd2) v |= kIsrCd2;
        if (p.cd1) v |= kIsrCd1;
        if (p.bvd2) v |= kIsrBvd2;
        if (p.bvd1) v |= kIsrBvd1;
        return v;
    }

    uint16_t ChangeStatusLocked(uint32_t ch) const {
        const uint16_t latched = this->StoredReg(ch * kChannelRegs + kCscr);
        uint16_t       v       = latched & (kCscrSoftwareDetect | kCscrDetectChange);
        if (!IoInterfaceLocked(ch)) {
            return static_cast<uint16_t>(v | (latched & (kCscrReadyChange | kCscrBatteryWarning | kCscrBatteryDead)));
        }
        v |= latched & kCscrStatusChange;
        if (IreqEdgeModeLocked()) return static_cast<uint16_t>(v | (latched & kCscrIreq));
        return ReadyPinHighLocked(ch) ? v : static_cast<uint16_t>(v | kCscrIreq);
    }

    uint32_t CausesLocked(uint32_t ch) const {
        const uint16_t status  = ChangeStatusLocked(ch);
        const uint16_t enables = this->StoredReg(ch * kChannelRegs + kCscier);
        uint32_t       causes  = status & enables & kCscierSourceEnables;
        if ((status & kCscrSoftwareDetect) != 0u && (enables & kCscierDetectEnable) != 0u) {
            causes |= kCauseSoftwareDetect;
        }
        if (IoInterfaceLocked(ch) && IreqModeLocked() != kIreqNone && (status & kCscrIreq) != 0u) causes |= kCauseIreq;
        return causes;
    }

    void LatchLocked(uint32_t ch, uint16_t bits) {
        const uint32_t i = ch * kChannelRegs + kCscr;
        this->SetStoredReg(i, static_cast<uint16_t>(this->StoredReg(i) | bits));
    }

    void SyncCardDetectLocked(uint32_t ch) {
        const bool card = !PresentLocked(ch, PinsLocked(ch)).cd1;
        if (card == card_detect_seen_[ch]) return;
        card_detect_seen_[ch] = card;
        LatchLocked(ch, kCscrDetectChange);
    }

    void TrackReadyPinLocked(uint32_t ch) {
        const bool high = PresentLocked(ch, PinsLocked(ch)).ready;
        if (high == ready_pin_high_[ch]) return;
        ready_pin_high_[ch] = high;
        if (!IoInterfaceLocked(ch)) {
            if (high) LatchLocked(ch, kCscrReadyChange);
            return;
        }
        const uint32_t mode = IreqModeLocked();
        if ((mode == kIreqFallingEdge && !high) || (mode == kIreqRisingEdge && high)) LatchLocked(ch, kCscrIreq);
    }

    void UpdateRequestLocked(uint32_t ch) { DeliverCausesLocked(ch, CausesLocked(ch)); }

    SocketDrive SocketDriveLocked(uint32_t ch) const {
        const uint16_t gcr = this->StoredReg(ch * kChannelRegs + kGcr);
        return SocketDrive{SocketPoweredLocked(ch), (gcr & kGcrCardReset) != 0u, (gcr & kGcrDriveEnable) != 0u};
    }

    void SetReadyIreqLow(PcmciaSlot& slot, bool low) {
        const uint32_t              ch = ChannelOf(slot);
        std::lock_guard<std::mutex> lk(this->lock_);
        ready_ireq_low_[ch] = low;
        TrackReadyPinLocked(ch);
        UpdateRequestLocked(ch);
    }

    PcmciaSlot  slot0_;
    PcmciaSlot  slot1_;
    PcmciaSlot* slots_[kChannels];
    bool        ready_ireq_low_[kChannels]   = {};
    bool        ready_pin_high_[kChannels]   = {true, true};
    bool        card_detect_seen_[kChannels] = {};
    bool        stschg_low_[kChannels]       = {};
};

}

}

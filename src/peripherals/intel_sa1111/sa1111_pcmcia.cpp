#include "sa1111_intc.h"
#include "sa1111_sbi.h"
#include "sa1111_system_controller.h"
#include "sa1111_unit.h"

#include "../pcmcia/pcmcia_slot.h"
#include "../pcmcia/pcmcia_space_router.h"

#include "../../core/board_parts.h"
#include "intel_sa1111_part_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/state/state_stream.h>

#include <mutex>

namespace cerfcore {

namespace {

constexpr char kPcCardSlotId[]    = "pc_card_slot";
constexpr char kPcCardSlotLabel[] = "PC Card slot";
constexpr char kCfSlotId[]        = "cf_slot";
constexpr char kCfSlotLabel[]     = "CF slot";

/* SA-1111 PCMCIA interface (Developer's Manual §12.6, base 0x40001800):
   PCCR +0x00 control, PCSSR +0x04 sleep state, PCSR +0x08 read-only
   status. */
constexpr uint32_t kOffPccr  = 0x00u;
constexpr uint32_t kOffPcssr = 0x04u;
constexpr uint32_t kOffPcsr  = 0x08u;

/* PCCR bits (§12.6.2). */
constexpr uint32_t kPccrS0Rst = 1u << 0;
constexpr uint32_t kPccrS0Flt = 1u << 2;

/* SA-1111 Developer's Manual §12.6.2 (printed 12-14): PCCR bits 7:0; §12.6.3 (printed
   12-15): PCSSR bits 1:0; Table 3-3 note 1: "All reserved bits are read back as zero." */
constexpr uint32_t kPccrDefined  = 0xFFu;
constexpr uint32_t kPcssrDefined = 0x3u;

/* PCSR bits (§12.6.1). */
constexpr uint32_t kPcsrS0Ready    = 1u << 0;
constexpr uint32_t kPcsrS1Ready    = 1u << 1;
constexpr uint32_t kPcsrS0CdInvalid = 1u << 2;   /* 0 = card present */
constexpr uint32_t kPcsrS1CdInvalid = 1u << 3;
constexpr uint32_t kPcsrS0Vs1 = 1u << 4;
constexpr uint32_t kPcsrS0Vs2 = 1u << 5;
constexpr uint32_t kPcsrS1Vs1 = 1u << 6;
constexpr uint32_t kPcsrS1Vs2 = 1u << 7;
constexpr uint32_t kPcsrS0Wp  = 1u << 8;
constexpr uint32_t kPcsrS1Wp  = 1u << 9;
constexpr uint32_t kPcsrS0Bvd1 = 1u << 10;
constexpr uint32_t kPcsrS0Bvd2 = 1u << 11;
constexpr uint32_t kPcsrS1Bvd1 = 1u << 12;
constexpr uint32_t kPcsrS1Bvd2 = 1u << 13;

/* SA-1111 interrupt sources (Developer's Manual Table 11-1): 49/50 =
   S0/S1 READY_nIREQ, 51/52 = S0/S1 CD valid change. */
constexpr uint8_t kIntS0Ready   = 49u;
constexpr uint8_t kIntS1Ready   = 50u;
constexpr uint8_t kIntS0CdValid = 51u;
constexpr uint8_t kIntS1CdValid = 52u;
constexpr uint8_t kIntS0Bvd1Stschg = 53u;
constexpr uint8_t kIntS1Bvd1Stschg = 54u;

class Sa1111Pcmcia : public Sa1111Unit, public PcmciaSlotHost {
public:
    explicit Sa1111Pcmcia(CerfEmulator& emu)
        : Sa1111Unit(emu),
          slot0_(emu, *this, {kPcCardSlotId, kPcCardSlotLabel}),
          slot1_(emu, *this, {kCfSlotId, kCfSlotLabel}),
          slots_{ &slot0_, &slot1_ } {}

    bool ShouldRegister() override {
        return emu_.Get<BoardParts>().Has(PartId::IntelSa1111);
    }

    void OnShutdown() override {
        slot0_.OnShutdown();
        slot1_.OnShutdown();
    }

    uint32_t MmioBase() const override { return 0x40001800u; }
    uint32_t MmioSize() const override { return 0x00000010u; }

    void SaveState(StateWriter& w) override {
        std::lock_guard<std::mutex> lk(state_mutex_);
        w.Write("pccr", pccr_);
        w.Write("pcssr", pcssr_);
        w.WriteBytes("ready_ireq_low",ready_ireq_low_, sizeof(ready_ireq_low_));
        w.WriteBytes("stschg_low", stschg_low_, sizeof(stschg_low_));
    }
    void RestoreState(StateReader& r) override {
        std::lock_guard<std::mutex> lk(state_mutex_);
        r.Read("pccr", pccr_);
        r.Read("pcssr", pcssr_);
        r.ReadBytes("ready_ireq_low",ready_ireq_low_, sizeof(ready_ireq_low_));
        r.ReadBytes("stschg_low", stschg_low_, sizeof(stschg_low_));
    }
    void PostRestore() override {
        UpdateResetSignals();
        for (int n = 0; n < 2; ++n) {
            DriveCardDetect(*slots_[n]);
            DrivePins(n);
        }
    }

    void OnCardDetectChanged(PcmciaSlot& slot) override {
        DriveCardDetect(slot);
        DrivePins(SocketOf(slot));
    }
    void OnCardPowerChanged(PcmciaSlot& slot) override { DrivePins(SocketOf(slot)); }

    void OnReadyIreqLow(PcmciaSlot& slot) override { SetReadyIreqLow(SocketOf(slot), true); }
    void OnReadyIreqHigh(PcmciaSlot& slot) override { SetReadyIreqLow(SocketOf(slot), false); }
    void OnStatusChange(PcmciaSlot& slot, bool low) override { SetStatusChangeLow(SocketOf(slot), low); }
    uint8_t UndrivenDataLane() const override { return kUnstatedUndrivenDataLane; }

protected:
    void OnUnitReady() override {
        emu_.Get<PcmciaSpaceRouter>().ProvideSockets(slots_[0], slots_[1]);
        auto& tree = emu_.Get<MachineObjectTree>();
        tree.Add(slots_[0]);
        tree.Add(slots_[1]);
        sbi_ = &emu_.Get<Sa1111Sbi>();
        emu_.Get<Sa1111SystemController>().RegisterClockListener(
            [this] { UpdateResetSignals(); });
        UpdateResetSignals();
    }

    uint32_t UnitReadWord(uint32_t addr) override {
        switch (addr - MmioBase()) {
            case kOffPccr:  {
                std::lock_guard<std::mutex> lk(state_mutex_);
                return pccr_;
            }
            case kOffPcssr: {
                std::lock_guard<std::mutex> lk(state_mutex_);
                return pcssr_;
            }
            case kOffPcsr:  return ComputePcsr();
        }
        HaltUnsupportedAccess("ReadWord", addr, 0);
    }

    void UnitWriteWord(uint32_t addr, uint32_t value) override {
        switch (addr - MmioBase()) {
            case kOffPccr:  WritePccr(value); return;
            case kOffPcssr: {
                {
                    std::lock_guard<std::mutex> lk(state_mutex_);
                    pcssr_ = value & kPcssrDefined;
                }
                UpdateResetSignals();
                return;
            }
        }
        HaltUnsupportedAccess("WriteWord", addr, value);
    }

    /* SA-1111 Developer's Manual §12.2.3: the socket reset signals "are asserted when the
       SA-1111 is in reset, or the relevant bit in the Control Register is set". */
    void OnChipReset(bool held) override {
        if (held) {
            {
                std::lock_guard<std::mutex> lk(state_mutex_);
                pccr_  = 0u;
                pcssr_ = 0u;
            }
            DriveCardDetect(*slots_[0]);
            DriveCardDetect(*slots_[1]);
            DrivePins(0);
            DrivePins(1);
        }
        UpdateResetSignals();
    }

private:
    int SocketOf(const PcmciaSlot& slot) const {
        return &slot == slots_[0] ? 0 : 1;
    }

    /* Table 12-1 (printed 12-5) S0_CDVALID: "1 = Card not fully inserted"; Table 12-4 (printed
       12-12): "The S1_CDVALID is used as an interrupt source". */
    void DriveCardDetect(const PcmciaSlot& slot) {
        emu_.Get<Sa1111Intc>().SetSourceLevel(SocketOf(slot) == 0 ? kIntS0CdValid : kIntS1CdValid,
                                              !slot.HasCard());
    }

    /* Table 12-3 (page 12-7) S0_READY_nIREQ: "Input from PC Card Socket RDY/nIRQ signal. This
       signal can be accessed through Status Register and used as an interrupt source." */
    bool ReadyPinLocked(int n) const { return !(slots_[n]->IsPowered() && ready_ireq_low_[n]); }

    bool CardDrivesPins(int n) const { return slots_[n]->HasCard() && slots_[n]->IsPowered(); }

    bool Bvd1PinLocked(int n) const { return !(slots_[n]->IsPowered() && stschg_low_[n]); }

    void PublishPinsLocked(int n) {
        Sa1111Intc& intc = emu_.Get<Sa1111Intc>();
        intc.SetSourceLevel(n == 0 ? kIntS0Ready : kIntS1Ready, ReadyPinLocked(n));
        intc.SetSourceLevel(n == 0 ? kIntS0Bvd1Stschg : kIntS1Bvd1Stschg, Bvd1PinLocked(n));
    }

    void SetStatusChangeLow(int n, bool low) {
        std::lock_guard<std::mutex> lk(state_mutex_);
        stschg_low_[n] = low;
        PublishPinsLocked(n);
    }

    void DrivePins(int n) {
        std::lock_guard<std::mutex> lk(state_mutex_);
        PublishPinsLocked(n);
    }

    void SetReadyIreqLow(int n, bool low) {
        std::lock_guard<std::mutex> lk(state_mutex_);
        ready_ireq_low_[n] = low;
        PublishPinsLocked(n);
    }

    uint32_t ComputePcsr() {
        bool ready0, ready1, bvd1_0, bvd1_1;
        {
            std::lock_guard<std::mutex> lk(state_mutex_);
            ready0 = ReadyPinLocked(0);
            ready1 = ReadyPinLocked(1);
            bvd1_0 = Bvd1PinLocked(0);
            bvd1_1 = Bvd1PinLocked(1);
        }
        uint32_t v = kPcsrS0Bvd2 | kPcsrS1Bvd2;
        if (bvd1_0) v |= kPcsrS0Bvd1;
        if (bvd1_1) v |= kPcsrS1Bvd1;
        if (!slots_[0]->Vs1Grounded()) v |= kPcsrS0Vs1;
        if (!slots_[0]->Vs2Grounded()) v |= kPcsrS0Vs2;
        if (!slots_[1]->Vs1Grounded()) v |= kPcsrS1Vs1;
        if (!slots_[1]->Vs2Grounded()) v |= kPcsrS1Vs2;
        if (!CardDrivesPins(0)) v |= kPcsrS0Wp;
        if (!CardDrivesPins(1)) v |= kPcsrS1Wp;
        if (!slots_[0]->HasCard()) v |= kPcsrS0CdInvalid;
        if (!slots_[1]->HasCard()) v |= kPcsrS1CdInvalid;
        if (ready0) v |= kPcsrS0Ready;
        if (ready1) v |= kPcsrS1Ready;
        return v;
    }

    void WritePccr(uint32_t value) {
        {
            std::lock_guard<std::mutex> lk(state_mutex_);
            pccr_ = value & kPccrDefined;
        }
        UpdateResetSignals();
    }

    /* Table 12-3 (printed 12-7) S0_RESET: "This signal needs a weak pull-up on board. The
       signal is active high"; "If PCCR<2> = 1 then {if in sleep mode, {if PCSSR<0> =0, This
       signal is tri-stated, elseif This signal is forced to low.} else S0_RESET = PCCR<0>}". */
    bool ResetAssertedLocked(int n) const {
        if (ChipHeld()) return true;
        if ((pccr_ & (kPccrS0Flt << n)) == 0u) return true;
        if (sbi_->SleepRequested()) return (pcssr_ & (1u << n)) == 0u;
        return (pccr_ & (kPccrS0Rst << n)) != 0u;
    }

    /* Table 12-3 (page 12-8) S0_nCE: "If PCCR<2> = 1 Then {if it is in sleep mode, {if PCSSR<0>
       =0, This signal is tri-stated, elseif This signal is forced to high.} ...} Else S0_nCE is
       tri-stated." */
    bool CardEnabledLocked(int n) const {
        return !ChipHeld() && (pccr_ & (kPccrS0Flt << n)) != 0u && !sbi_->SleepRequested();
    }

    void UpdateResetSignals() {
        bool asserted[2] = {};
        bool enabled[2]  = {};
        {
            std::lock_guard<std::mutex> lk(state_mutex_);
            for (int n = 0; n < 2; ++n) {
                asserted[n] = ResetAssertedLocked(n);
                enabled[n]  = CardEnabledLocked(n);
            }
        }
        for (int n = 0; n < 2; ++n) {
            slots_[n]->SetCardEnabled(enabled[n]);
            slots_[n]->SetResetLine(asserted[n]);
        }
    }

    PcmciaSlot  slot0_;
    PcmciaSlot  slot1_;
    PcmciaSlot* slots_[2];

    const Sa1111Sbi* sbi_ = nullptr;

    std::mutex state_mutex_;
    uint32_t   pccr_  = 0u;
    uint32_t   pcssr_ = 0u;
    bool       ready_ireq_low_[2] = {};
    bool       stschg_low_[2]     = {};
};

}  /* namespace */

REGISTER_SERVICE(Sa1111Pcmcia);

}

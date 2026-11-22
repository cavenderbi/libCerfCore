#pragma once

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include <cerfengine/sh/sh_cpu_state.h>
#include <cerfengine/sh/sh_interrupt_channel.h>
#include <cerfengine/sh/sh_interrupt_host.h>
#include "../../execution/engine_bridge.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "sh77xx_area1.h"
#include "sh77xx_intc.h"
#include "sh77xx_intc_absent_inputs.h"
#include "sh77xx_intc_input_pins.h"
#include "sh77xx_intc_model.h"
#include "sh77xx_nmi_input.h"
#include "sh77xx_rtc_oscillator.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <string_view>

namespace cerfcore {

namespace sh77xx_intc {

template <const std::string_view& Soc, const Model& M>
class Sh77xxIntcBase : public Sh77xxIntc, public cerfengine::ShInterruptHost {
public:
    using Sh77xxIntc::Sh77xxIntc;

    bool ShouldRegister() override { return this->emu_.template Get<BoardContext>().GetSocId() == Soc; }

    void OnReady() override {
        nmi_     = &this->emu_.template Get<Sh77xxNmiInput>();
        pins_    = &this->emu_.template Get<Sh77xxIntcInputPins>();
        reset_   = &this->emu_.template Get<GuestCpuReset>();
        channel_ = &this->emu_.template Get<EngineBridge>().Engine().template Get<cerfengine::ShInterruptChannel>();
        ResetRegisters();
        channel_->SetHost(this);
        this->emu_.template Get<PeripheralDispatcher>().Register(this);
        reset_->RegisterResetListener([this](ResetLineKind) {
            std::lock_guard<std::mutex> lk(mtx_);
            ResetRegisters();
            PublishLocked();
        });
    }

    uint32_t MmioBase() const override { return M.area1_registers ? kSh77xxArea1PaBase : kSh77xxIntcArea7Base; }
    uint32_t MmioSize() const override {
        if (!M.area1_registers) return kSh77xxIntcArea7Size + (M.area7_extra_ipr ? kArea7ExtraIprBytes : 0u);
        return kArea1Size;
    }

    void SetSourceRequest(Sh77xxIntcSource source, bool asserted) override {
        std::lock_guard<std::mutex> lk(mtx_);
        const uint64_t bit = SourceBit(source);
        if (((requests_ & bit) != 0u) == asserted) return;
        requests_ ^= bit;
        PublishLocked();
    }

    void SetPinLevel(Sh77xxIntcPin pin, bool high) override {
        std::lock_guard<std::mutex> lk(mtx_);
        pins_->SetNet(1u << static_cast<uint32_t>(pin), high);
        ApplyPinLevelsLocked();
    }

    void SetPortDrive(uint32_t driven_pins, uint32_t driven_high, uint32_t absent_input_pins,
                      uint32_t absent_irl_pins) override {
        std::lock_guard<std::mutex> lk(mtx_);
        pins_->SetDrive(driven_pins, driven_high);
        absent_ = AbsentInputs{absent_input_pins, absent_irl_pins};
        ApplyPinLevelsLocked();
    }

    bool PinLevel(Sh77xxIntcPin pin) override {
        std::lock_guard<std::mutex> lk(mtx_);
        return pins_->High(static_cast<uint32_t>(pin));
    }

    void SetNmiLevel(bool high) override {
        std::lock_guard<std::mutex> lk(mtx_);
        nmi_->SetLevel(high);
        PublishLocked();
    }

    uint32_t AcceptInterrupt(uint32_t sr) override {
        std::lock_guard<std::mutex> lk(mtx_);
        const Selection s   = SelectLocked(Scope::kAll);
        const bool      nmi = s.level == kNmiLevel;
        if (nmi && BlmskLocked()) sr &= ~cerfengine::kShSrBl;
        if (!cerfengine::ShSrAcceptsInterrupt(sr, s.level)) return 0u;
        if (!nmi) HaltOnAbsentInputLocked("interrupt acceptance", ObservedAbsentInputs(Scope::kAll));
        intevt2_ = s.intevt2;
        if (nmi) {
            nmi_->Accept();
            PublishLocked();
        }
        return s.intevt;
    }

    uint32_t StandbyWakeLevel() override { return StandbyWakeLevelWith(0u, 0u); }

    bool WakesStandby(Sh77xxIntcSource source) override {
        std::lock_guard<std::mutex> lk(mtx_);
        if (((kStandbyWakeModules | standby_wake_sources_) & SourceBit(source)) == 0u) return false;
        if (AllMaskedLocked()) return false;
        return channel_->LevelEndsSleep(RowLevel(kModuleRows[static_cast<size_t>(source)]));
    }

    bool PintWakesStandby(uint32_t pint, bool high) override {
        const uint32_t bit = 1u << pint;
        return channel_->LevelEndsSleep(StandbyWakeLevelWith(bit, high ? bit : 0u));
    }

    void SetStandbyWake(Sh77xxIntcSource source, bool wakes) override {
        std::lock_guard<std::mutex> lk(mtx_);
        if (wakes) {
            standby_wake_sources_ |= SourceBit(source);
        } else {
            standby_wake_sources_ &= ~SourceBit(source);
        }
    }

    uint8_t ReadByte(uint32_t addr) override {
        std::lock_guard<std::mutex> lk(mtx_);
        switch (addr - kSh77xxArea1PaBase) {
        case kOffIrr0: return ReadIrr0Locked(addr);
        case kOffIrr1: return static_cast<uint8_t>(requests_ & kIrr1Mask);
        case kOffIrr2:
            return static_cast<uint8_t>((requests_ >> kIrr2Shift) & (M.lcdc_pcc ? kIrr2LcdiMask : kIrr2Mask));
        case kOffIrr3:
            if (!M.lcdc_pcc) break;
            return static_cast<uint8_t>((requests_ >> kIrr3Shift) & kIrr3Mask);
        case kOffIrr4:
            if (!M.lcdc_pcc) break;
            return static_cast<uint8_t>((requests_ >> kIrr4Shift) & kIrr4Mask);
        default: break;
        }
        this->HaltUnsupportedAccess("ReadByte", addr, 0u);
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        const uint32_t off = addr - kSh77xxArea1PaBase;
        if (off == kOffIrr1 || off == kOffIrr2) return;
        if (off != kOffIrr0) this->HaltUnsupportedAccess("WriteByte", addr, value);
        std::lock_guard<std::mutex> lk(mtx_);
        pins_->ClearIrr0(value);
        PublishLocked();
    }

    uint16_t ReadHalf(uint32_t addr) override {
        if (!M.area1_registers) return ReadArea7Half(addr);
        std::lock_guard<std::mutex> lk(mtx_);
        switch (addr - kSh77xxArea1PaBase) {
        case kOffIrr1:
        case kOffIrr2:   return 0u;
        case kOffIcr1:   return icr1_;
        case kOffIcr2:   return icr2_;
        case kOffPinter: return pinter_;
        case kOffIprc:   return ipr_[kIprc];
        case kOffIprd:   return ipr_[kIprd];
        case kOffIpre:   return ipr_[kIpre];
        case kOffIprf:
            if (!M.lcdc_pcc) break;
            return ipr_[kIprf];
        default:         break;
        }
        this->HaltUnsupportedAccess("ReadHalf", addr, 0u);
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        if (!M.area1_registers) return WriteArea7Half(addr, value);
        std::lock_guard<std::mutex> lk(mtx_);
        switch (addr - kSh77xxArea1PaBase) {
        case kOffIrr1:
        case kOffIrr2:   return;
        case kOffIcr1:   WriteIcr1Locked(addr, value); break;
        case kOffIcr2:   icr2_ = value; break;
        case kOffPinter:
            if ((value & absent_.Pints()) != 0u) {
                this->HaltUnsupportedAccess("WriteHalf PINTER enables a PINT input that is not on its pin", addr, value);
            }
            pinter_ = value;
            break;
        case kOffIprc:   ipr_[kIprc] = value; break;
        case kOffIprd:   ipr_[kIprd] = value; break;
        case kOffIpre:   ipr_[kIpre] = value; break;
        case kOffIprf:
            if ((value & M.IprfHaltBits()) != 0u) this->HaltUnsupportedAccess("WriteHalf sets a reserved bit", addr, value);
            ipr_[kIprf] = value;
            break;
        default:         this->HaltUnsupportedAccess("WriteHalf", addr, value);
        }
        PublishLocked();
    }

    uint32_t ReadWord(uint32_t addr) override {
        if (addr - kSh77xxArea1PaBase != kOffIntevt2) this->HaltUnsupportedAccess("ReadWord", addr, 0u);
        std::lock_guard<std::mutex> lk(mtx_);
        return intevt2_;
    }

    uint16_t ReadArea7Half(uint32_t addr) override {
        std::lock_guard<std::mutex> lk(mtx_);
        switch (addr - kSh77xxIntcArea7Base) {
        case kOffIcr0:
            return static_cast<uint16_t>((nmi_->High() ? kIcr0Nmil : 0u) | (nmi_->Nmie() ? kIcr0Nmie : 0u));
        case kOffIpra: return ipr_[kIpra];
        case kOffIprb: return ipr_[kIprb];
        case kOffArea7ExtraIpr:
            if (!M.area7_extra_ipr || !area7_extra_written_) break;
            return ipr_[kIprArea7Extra];
        default: break;
        }
        this->HaltUnsupportedAccess("ReadHalf", addr, 0u);
    }

    void WriteArea7Half(uint32_t addr, uint16_t value) override {
        std::lock_guard<std::mutex> lk(mtx_);
        switch (addr - kSh77xxIntcArea7Base) {
        case kOffIcr0:
            WriteIcr0Locked(value);
            break;
        case kOffIpra:
            ipr_[kIpra] = value;
            break;
        case kOffIprb:
            if ((value & kIprbReserved) != 0u) this->HaltUnsupportedAccess("WriteHalf IPRB bits 3-0", addr, value);
            ipr_[kIprb] = value;
            break;
        case kOffArea7ExtraIpr:
            if (!M.area7_extra_ipr) this->HaltUnsupportedAccess("WriteHalf", addr, value);
            WriteArea7ExtraIprLocked(addr, value);
            break;
        default:
            this->HaltUnsupportedAccess("WriteHalf", addr, value);
        }
        PublishLocked();
    }

    void SaveState(StateWriter& w) override {
        std::lock_guard<std::mutex> lk(mtx_);
        w.Write("icr1", icr1_);
        w.Write("icr2", icr2_);
        w.Write("pinter", pinter_);
        w.WriteBytes("ipr", ipr_, sizeof(ipr_));
        w.Write("source_requests", requests_);
        w.Write("intevt2", intevt2_);
        pins_->SaveState(w);
        nmi_->SaveState(w);
        w.Write("area7_extra_ipr_written", area7_extra_written_);
    }

    void RestoreState(StateReader& r) override {
        std::lock_guard<std::mutex> lk(mtx_);
        r.Read("icr1", icr1_);
        r.Read("icr2", icr2_);
        r.Read("pinter", pinter_);
        r.ReadBytes("ipr", ipr_, sizeof(ipr_));
        r.Read("source_requests", requests_);
        r.Read("intevt2", intevt2_);
        pins_->RestoreState(r);
        nmi_->RestoreState(r);
        r.Read("area7_extra_ipr_written", area7_extra_written_);
    }

    void PostRestore() override {
        std::lock_guard<std::mutex> lk(mtx_);
        nmi_->PostRestore();
        PublishLocked();
    }

private:
    static void Offer(Selection& best, uint32_t level, uint32_t intevt, uint32_t intevt2) {
        if (level <= best.level) return;
        best = Selection{level, intevt, intevt2};
    }

    void PublishLocked() {
        const uint32_t level = SelectLocked(Scope::kAll).level;
        channel_->PublishLevel(level, level == kNmiLevel && BlmskLocked());
    }

    bool MaiLocked() const { return M.area1_registers && (icr1_ & kIcr1Mai) != 0u; }
    bool BlmskLocked() const { return M.area1_registers && (icr1_ & kIcr1Blmsk) != 0u; }
    bool NmiOfferedLocked() const { return nmi_->Pending() && !MaiLocked(); }

    uint32_t ObservedAbsentInputs(Scope scope) const {
        uint32_t irqs = M.SelectableIrqPins(scope, IrlPins());
        for (uint32_t n = 0; n < kIrqPins; ++n) {
            if (((irqs >> n) & 1u) != 0u && RowLevel(kIrqRows[n]) == 0u) irqs &= ~(1u << n);
        }
        uint32_t pints = 0u;
        if (M.SelectsPint(scope)) {
            for (size_t g = 0; g < std::size(kPintRows); ++g) {
                if (RowLevel(kPintRows[g]) != 0u) pints |= pinter_ & kPintGroupBits[g];
            }
        }
        return absent_.Observed(irqs, pints, IrlMode(), IrlsInUse());
    }

    void HaltOnAbsentInputLocked(const char* observation, uint32_t inputs) const {
        if (inputs != 0u) {
            this->emu_.template Get<Fatal>().Die("Sh77xxIntc: %s with the interrupt inputs 0x%X not on their pins",
                                                 observation, inputs);
        }
    }

    void ResetRegisters() {
        icr1_           = kIcr1Reset;
        icr2_           = 0u;
        pinter_         = 0u;
        pins_->ResetLatches();
        area7_extra_written_ = false;
        nmi_->Reset();
        for (uint16_t& ipr : ipr_) ipr = 0u;
    }

    void WriteArea7ExtraIprLocked(uint32_t addr, uint16_t value) {
        if ((value & kArea7ExtraReserved) != 0u) {
            this->HaltUnsupportedAccess("WriteHalf extra IPR bits 3-0", addr, value);
        }
        if (((value >> kArea7ExtraHighShift) & kMaxLevel) != ((value >> kArea7ExtraMidShift) & kMaxLevel)) {
            this->HaltUnsupportedAccess("WriteHalf extra IPR with unequal bits 15-12 and 11-8", addr, value);
        }
        ipr_[kIprArea7Extra]   = value;
        area7_extra_written_ = true;
    }

    void WriteIcr0Locked(uint16_t value) { nmi_->WriteNmie((value & kIcr0Nmie) != 0u); }

    void WriteIcr1Locked(uint32_t addr, uint16_t value) {
        if (!M.irls) value = static_cast<uint16_t>(value & ~kIcr1Irlsen);
        for (uint32_t n = 0; n < kIrqPins; ++n) {
            if (((value >> (2u * n)) & 3u) == kSenseReserved) {
                this->HaltUnsupportedAccess("WriteHalf ICR1 IRQ sense 11", addr, value);
            }
        }
        icr1_ = value;
        if (absent_.IrlInUse(IrlMode(), IrlsInUse()) != 0u) {
            this->HaltUnsupportedAccess("WriteHalf ICR1 selects an IRL or IRLS input that is not on its pin", addr,
                                        value);
        }
    }

    uint8_t ReadIrr0Locked(uint32_t addr) {
        if ((icr1_ & kIcr1IrqLevel) != 0u) this->HaltUnsupportedAccess("ReadByte IRR0 with ICR1.IRQLVL set", addr, 0u);
        HaltOnAbsentInputLocked("IRR0 read", absent_.IrqPins());
        return pins_->ReadIrr0(icr1_, icr2_, pinter_, absent_.Pints());
    }

    void ApplyPinLevelsLocked() {
        if (pins_->Apply(icr1_, IrlPins(), reset_->Delivering())) PublishLocked();
    }

    bool     IrlMode() const { return !M.irq_pins || (icr1_ & kIcr1IrqLevel) != 0u; }
    uint32_t IrlPins() const { return IrlMode() ? (1u << kIrlPins) - 1u : 0u; }
    bool     IrlsInUse() const { return M.irls && (icr1_ & kIcr1Irlsen) != 0u; }

    uint32_t RowLevel(const SourceRow& row) const { return (ipr_[row.ipr] >> row.shift) & kMaxLevel; }

    void OfferRow(Selection& best, const SourceRow& row) const {
        if (row.ipr == kIprArea7Extra && !area7_extra_written_) {
            this->emu_.template Get<Fatal>().Die("Sh77xxIntc: request of INTEVT 0x%03X while the extra IPR is not "
                                                 "written since the reset", row.intevt2);
        }
        const uint32_t level = RowLevel(row);
        Offer(best, level, row.level_coded ? LevelCode(level) : row.intevt2, row.intevt2);
    }

    bool AllMaskedLocked() const { return MaiLocked() && !nmi_->High(); }

    uint32_t StandbyWakeLevelWith(uint32_t pint_override, uint32_t pint_override_high) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (NmiOfferedLocked()) return kNmiLevel;
        }
        const Sh77xxRtcOscillator* rtc = this->emu_.template TryGet<Sh77xxRtcOscillator>();
        if (rtc == nullptr) {
            this->emu_.template Get<Fatal>().Die("Sh77xxIntc: a standby wake on %.*s needs the RTC oscillator, "
                                                 "and no RTC model exists for it",
                                                 static_cast<int>(Soc.size()), Soc.data());
        }
        if (!rtc->OscillatorRunning()) return 0u;
        std::lock_guard<std::mutex> lk(mtx_);
        HaltOnAbsentInputLocked("standby wake", ObservedAbsentInputs(Scope::kStandbyWake));
        return SelectLocked(Scope::kStandbyWake, pint_override, pint_override_high).level;
    }

    Selection SelectLocked(Scope scope, uint32_t pint_override = 0u, uint32_t pint_override_high = 0u) const {
        Selection best;
        if (AllMaskedLocked()) return best;
        if (NmiOfferedLocked()) best = Selection{kNmiLevel, kNmiIntevt, kNmiIntevt};
        const bool     standby   = scope == Scope::kStandbyWake;
        if (standby && (requests_ & kArea7ExtraSources) != 0u) {
            this->emu_.template Get<Fatal>().Die("Sh77xxIntc: a request of an extra IPR source in standby (requests 0x%llX)",
                                                 static_cast<unsigned long long>(requests_));
        }
        const uint32_t irq_pins = M.SelectableIrqPins(scope, IrlPins());
        const uint64_t modules  = standby ? (kStandbyWakeModules | standby_wake_sources_) : ~uint64_t{0};
        if (IrlMode()) {
            const uint32_t irl = pins_->IrlCode(IrlsInUse());
            if (irl != kIrlNone) {
                const uint32_t level = kMaxLevel - irl;
                Offer(best, level, LevelCode(level), LevelCode(level));
            }
        }
        for (uint32_t n = 0; n < kIrqPins; ++n) {
            if (((irq_pins >> n) & 1u) != 0u && pins_->IrqRequest(icr1_, n)) OfferRow(best, kIrqRows[n]);
        }
        if (M.SelectsPint(scope)) {
            const uint32_t pint = pins_->PintRequests(icr2_, pinter_, pint_override, pint_override_high);
            for (size_t g = 0; g < std::size(kPintRows); ++g) {
                if ((pint & kPintGroupBits[g]) != 0u) OfferRow(best, kPintRows[g]);
            }
        }
        const uint64_t requests = requests_ & modules;
        for (size_t i = 0; i < std::size(kModuleRows); ++i) {
            if ((requests & (uint64_t{1} << i)) != 0u) OfferRow(best, kModuleRows[i]);
        }
        return best;
    }

    std::mutex                      mtx_;
    Sh77xxNmiInput*                 nmi_            = nullptr;
    GuestCpuReset*                  reset_          = nullptr;
    cerfengine::ShInterruptChannel* channel_        = nullptr;
    uint16_t                        icr1_           = 0u;
    uint16_t                        icr2_           = 0u;
    uint16_t                        pinter_         = 0u;
    uint16_t                        ipr_[kIprCount] = {};
    uint64_t                        requests_       = 0u;
    uint64_t                        standby_wake_sources_ = 0u;
    bool                            area7_extra_written_ = false;
    uint32_t                        intevt2_        = 0u;
    Sh77xxIntcInputPins*            pins_           = nullptr;
    AbsentInputs                    absent_;
};

}

}

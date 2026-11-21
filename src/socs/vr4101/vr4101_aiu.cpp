#include "../vr41xx/vr41xx_aiu_dma.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/hibernation.h>
#include <cerfcore/state/state_stream.h>
#include "../../cpu/mips/mips_core_clock.h"
#include "../../execution/guest_cycle_clock.h"
#include "../../host/host_pcm_queue.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../guest_sample_clock.h"
#include "../oscillator_ticks.h"
#include "../vr41xx/vr41xx_aiu_interrupts.h"
#include "../vr41xx/vr41xx_cmu.h"
#include "../vr41xx/vr41xx_dma_page_interrupts.h"
#include "../vr41xx/vr41xx_unit_clock_gate.h"
#include "vr4101_aiu_output.h"
#include "vr4101_dma_addresses.h"
#include "vr4101_dma_control.h"
#include "vr4101_id.h"
#include "vr4101_interval_unit.h"

#include <algorithm>
#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x0B000160u;
constexpr uint32_t kSize = 0x20u;

constexpr uint32_t kOffDat     = 0x02u;
constexpr uint32_t kOffReset   = 0x04u;
constexpr uint32_t kOffMode    = 0x06u;
constexpr uint32_t kOffSeqEn   = 0x08u;
constexpr uint32_t kOffMute    = 0x0Au;
constexpr uint32_t kOffStat    = 0x0Cu;
constexpr uint32_t kOffStpPage = 0x0Eu;
constexpr uint32_t kOffValid   = 0x10u;
constexpr uint32_t kOffInt     = 0x12u;
constexpr uint32_t kOffCount0  = 0x14u;
constexpr uint32_t kOffCount1  = 0x16u;
constexpr uint32_t kOffRepNum  = 0x18u;
constexpr uint32_t kOffBusEn   = 0x1Au;

constexpr uint16_t kFlagBit   = 0x0001u;
constexpr uint16_t kPwmMode   = 0x0001u;
constexpr uint16_t kMuteBits  = 0x0003u;
constexpr uint16_t kRepBits   = 0x0007u;
constexpr uint16_t kWholeWord = 0xFFFFu;
constexpr uint16_t kIntStart  = 1u << 0;
constexpr uint16_t kIntIdle   = 1u << 1;
constexpr uint16_t kIntPage   = 1u << 2;
constexpr uint16_t kIntEnd    = 1u << 3;
constexpr uint16_t kIntBits   = kIntStart | kIntIdle | kIntPage | kIntEnd;
constexpr uint16_t kIntWrite  = 0x00FFu;
constexpr uint64_t kNever     = UINT64_MAX;

constexpr Vr41xxDmaPageInterrupts kPages{kIntPage, kIntEnd};
constexpr Vr4101DmaChannel        kChannel = Vr4101DmaChannel::kAiu;
constexpr uint16_t kDrqAiu = static_cast<uint16_t>(1u << static_cast<uint32_t>(kChannel));

enum Segment : uint8_t { kNoSegment = 0u, kPwmDma = 1u, kPwmCpu = 2u, kBuzz = 3u };

class Vr4101Aiu : public Vr41xxAiuDma {
public:
    using Vr41xxAiuDma::Vr41xxAiuDma;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4101; }

    void OnReady() override;
    void OnShutdown() override { out_.Stop(); }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    uint32_t ReadWord(uint32_t addr) override { return ReadWordAsHalves(addr); }
    void     WriteWord(uint32_t addr, uint32_t value) override { WriteWordAsHalves(addr, value); }

    uint16_t ReadHalf(uint32_t addr) override {
        SyncDma();
        switch (addr - kBase) {
            case kOffDat:
            case kOffStat:
            case kOffReset:   return 0u;
            case kOffValid:   return cpu_valid_ ? kFlagBit : 0u;
            case kOffMode:    return mode_;
            case kOffSeqEn:   return seqen_;
            case kOffMute:    return mute_;
            case kOffStpPage: return stppage_;
            case kOffInt:     return ints_.Bits();
            case kOffCount0:  return count0_;
            case kOffCount1:  return count1_;
            case kOffRepNum:  return rep_;
            case kOffBusEn:   return busen_;
            default:          HaltUnsupportedAccess("VR4101 AIU ReadHalf", addr, 0);
        }
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        WriteDmaRegister([&] { WriteRegister(addr, value); });
    }

    void SyncDma() override { CatchUp(); }

    uint16_t DmaRequests() override {
        const bool wants = seqen_ && (mode_ & kPwmMode) && busen_ && seg_ == kNoSegment && !StoppedAtPage();
        return wants && !dcu_->TransferEnabled(kChannel) ? kDrqAiu : 0u;
    }

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

private:
    void DmaRegistersWritten() override {
        if (seqen_ && seg_ == kNoSegment && gate_.Running()) {
            cursor_ = ticks_.Now();
            StartSegment();
        }
        Rearm();
    }

    static uint16_t Writable(uint32_t off) {
        switch (off) {
            case kOffDat:
            case kOffStat:
            case kOffValid:
            case kOffCount0:
            case kOffCount1:  return kWholeWord;
            case kOffReset:
            case kOffMode:
            case kOffSeqEn:
            case kOffStpPage:
            case kOffBusEn:   return kFlagBit;
            case kOffMute:    return kMuteBits;
            case kOffInt:     return kIntWrite;
            case kOffRepNum:  return kRepBits;
            default:          return 0u;
        }
    }

    void WriteRegister(uint32_t addr, uint16_t value) {
        const uint32_t off = addr - kBase;
        if (Writable(off) == 0u || (value & ~Writable(off)) != 0u) HaltUnsupportedAccess("VR4101 AIU WriteHalf", addr, value);
        switch (off) {
            case kOffDat:
            case kOffStat:    return;
            case kOffValid:   cpu_valid_ = (value & kFlagBit) != 0u; return;
            case kOffReset:
                if (value != 0u) ResetUnit();
                return;
            case kOffMode:    mode_ = value; return;
            case kOffSeqEn:   SetSequencer(value != 0u); return;
            case kOffMute:    mute_ = value; return;
            case kOffStpPage: stppage_ = value; return;
            case kOffInt:     ClearInt(value); return;
            case kOffCount0:  count0_ = value; return;
            case kOffCount1:  count1_ = value; return;
            case kOffRepNum:  rep_ = value; return;
            case kOffBusEn:
                busen_ = value;
                if (!busen_ && seg_ == kPwmDma) seg_ = kNoSegment;
                return;
            default:          HaltUnsupportedAccess("VR4101 AIU WriteHalf", addr, value);
        }
    }

    bool StoppedAtPage() const { return stop_one_page_ || stop_full_; }
    bool DmaFetchAllowed() const { return dcu_->TransferEnabled(kChannel) && !StoppedAtPage(); }

    uint64_t PairTicks(uint16_t high, uint16_t low) const {
        return std::max<uint64_t>((static_cast<uint64_t>(high) + low) * (static_cast<uint64_t>(rep_) + 1u), 1u);
    }

    void SetSequencer(bool on);
    bool StartSegment();
    void BeginPair(Segment kind, uint64_t length);
    void NoSegment();
    void OutputUntil(uint64_t t);
    void PageBoundary();
    void CatchUp();
    void Rearm();
    uint64_t NextInterruptAt() const;
    uint64_t TicksToBoundaryFetch() const;
    void ClearInt(uint16_t bits);
    void ResetUnit();
    void ApplyClock();

    StoppableOscillatorTicks ticks_{emu_, false};
    GuestSampleClock         flush_{emu_};
    Vr4101AiuOutput          out_;
    Vr41xxAiuInterrupts      ints_{emu_};
    Vr41xxUnitClockGate      gate_{emu_};
    GuestCycleClock*         clock_ = nullptr;
    GuestCycleClock::Event*  event_ = nullptr;
    Vr4101DmaAddresses*      dma_   = nullptr;
    Vr4101DmaControl*        dcu_   = nullptr;
    EmulatedMemory*          mem_   = nullptr;
    uint16_t mode_    = 0u;
    uint16_t seqen_   = 0u;
    uint16_t mute_    = 0u;
    uint16_t stppage_ = 0u;
    uint16_t rep_     = 0u;
    uint16_t busen_   = 0u;
    uint16_t count0_  = 0u;
    uint16_t count1_  = 0u;
    uint16_t pair_high_ = 0u;
    uint16_t pair_low_  = 0u;
    uint8_t  seg_       = kNoSegment;
    uint64_t cursor_    = 0u;
    uint64_t seg_start_ = 0u;
    uint64_t seg_end_   = 0u;
    uint64_t half_      = 0u;
    bool     primed_        = false;
    bool     idle_          = false;
    bool     buzz_done_     = false;
    bool     cpu_valid_     = false;
    bool     stop_one_page_ = false;
    bool     stop_full_     = false;
};

void Vr4101Aiu::OnReady() {
    emu_.Get<PeripheralDispatcher>().Register(this);
    dma_   = &emu_.Get<Vr4101DmaAddresses>();
    dcu_   = &emu_.Get<Vr4101DmaControl>();
    mem_   = &emu_.Get<EmulatedMemory>();
    clock_ = &emu_.Get<GuestCycleClock>();
    ints_.Attach();

    auto&                       core   = emu_.Get<MipsCoreClock>();
    const GuestCycleClock::Rate pclock = core.ResetRate();
    ticks_.Attach(pclock.num, pclock.den * core.CyclesPerTclkCounterTick());
    event_ = clock_->Add([this] {
        CatchUp();
        Rearm();
    });
    flush_.Attach([this] { CatchUp(); });
    out_.Attach(emu_, ticks_.OscRate());

    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { ResetUnit(); });
    gate_.Attach(kCmuMskAiu, [this] { ApplyClock(); });
    clock_->RegisterRateListener([this] {
        ticks_.Rescale();
        Rearm();
    });
    emu_.Get<Hibernation>().RegisterPreSaveListener([this] { SyncDma(); });
    gate_.Refresh();
}

void Vr4101Aiu::SetSequencer(bool on) {
    if (on == (seqen_ != 0u)) return;
    if (!on) {
        seqen_ = 0u;
        seg_   = kNoSegment;
        out_.Finish();
        flush_.Stop();
        clock_->Disarm(event_);
        return;
    }
    seqen_     = 1u;
    cursor_    = ticks_.Now();
    seg_       = kNoSegment;
    primed_    = false;
    idle_      = false;
    buzz_done_ = false;
    out_.Begin();
    flush_.Start(kHostPcmFlushRate);
}

bool Vr4101Aiu::StartSegment() {
    if (mode_ & kPwmMode) {
        if (!busen_) {
            if (!cpu_valid_) {
                NoSegment();
                return false;
            }
            cpu_valid_ = false;
            BeginPair(kPwmCpu, PairTicks(count0_, count1_));
            return true;
        }
        if (!DmaFetchAllowed()) {
            NoSegment();
            return false;
        }
        count0_       = dma_->Load(kChannel);
        bool boundary = dma_->Step(kChannel);
        count1_       = dma_->Load(kChannel);
        boundary      = dma_->Step(kChannel) || boundary;
        BeginPair(kPwmDma, PairTicks(count0_, count1_));
        if (boundary) PageBoundary();
        return true;
    }
    if (buzz_done_) {
        NoSegment();
        return false;
    }
    buzz_done_ = true;
    half_      = static_cast<uint64_t>(count0_) * kVr4101TclocksPerIntervalUnit;
    BeginPair(kBuzz, static_cast<uint64_t>(count1_) * kVr4101TclocksPerIntervalUnit);
    return true;
}

void Vr4101Aiu::BeginPair(Segment kind, uint64_t length) {
    seg_       = kind;
    seg_start_ = cursor_;
    seg_end_   = cursor_ + length;
    pair_high_ = count0_;
    pair_low_  = count1_;
    idle_      = false;
    if (!primed_) {
        primed_ = true;
        ints_.Raise(kIntStart);
    }
}

void Vr4101Aiu::NoSegment() {
    seg_ = kNoSegment;
    if (primed_ && !idle_) {
        idle_ = true;
        ints_.Raise(kIntIdle);
    }
}

void Vr4101Aiu::OutputUntil(uint64_t t) {
    if (seg_ == kBuzz) out_.AddSquare(seg_start_, half_, mute_, cursor_, t);
    else               out_.AddPwm(pair_high_, pair_low_, mute_, t - cursor_);
    cursor_ = t;
}

void Vr4101Aiu::PageBoundary() {
    const uint16_t raised = kPages.AtBoundary(ints_.Bits());
    if (raised & kIntEnd) stop_full_ = true;
    if (stppage_ & kFlagBit) stop_one_page_ = true;
    ints_.Raise(raised);
}

void Vr4101Aiu::CatchUp() {
    if (!seqen_) return;
    const uint64_t now = ticks_.Now();
    for (;;) {
        if (seg_ == kNoSegment && !StartSegment()) {
            cursor_ = now;
            break;
        }
        if (seg_end_ > now) {
            OutputUntil(now);
            break;
        }
        OutputUntil(seg_end_);
        seg_ = kNoSegment;
    }
    out_.Flush();
}

uint64_t Vr4101Aiu::NextInterruptAt() const {
    switch (seg_) {
        case kPwmDma:
            if ((mode_ & kPwmMode) && busen_ && DmaFetchAllowed()) return cursor_ + TicksToBoundaryFetch();
            return seg_end_;
        case kPwmCpu:
        case kBuzz:   return seg_end_;
        default:      return kNever;
    }
}

uint64_t Vr4101Aiu::TicksToBoundaryFetch() const {
    const uint32_t before = (dma_->HalfwordsToBoundary(kChannel) + 1u) / 2u - 1u;
    const uint32_t pa     = dma_->Address(kChannel);
    uint64_t       ticks  = seg_end_ - cursor_;
    for (uint32_t i = 0; i < before; ++i) {
        uint8_t raw[4];
        if (mem_->TryCopyOut(pa + 4u * i, raw, 4u) != 4u) break;
        ticks += PairTicks(cerf::le::U16(raw), cerf::le::U16(raw + 2));
    }
    return ticks;
}

void Vr4101Aiu::Rearm() {
    const uint64_t at = seqen_ && gate_.Running() ? NextInterruptAt() : kNever;
    if (at == kNever) {
        clock_->Disarm(event_);
        return;
    }
    ticks_.ArmAt(event_, at);
}

void Vr4101Aiu::ClearInt(uint16_t bits) {
    const uint16_t cleared = ints_.Clear(static_cast<uint16_t>(bits & kIntBits));
    if (cleared & kIntPage) stop_one_page_ = false;
    if (cleared & kIntEnd) stop_full_ = false;
}

void Vr4101Aiu::ResetUnit() {
    if (seqen_) out_.Finish();
    flush_.Stop();
    mode_ = seqen_ = mute_ = stppage_ = rep_ = busen_ = count0_ = count1_ = 0u;
    pair_high_ = pair_low_ = 0u;
    seg_ = kNoSegment;
    cursor_ = seg_start_ = seg_end_ = half_ = 0u;
    primed_ = idle_ = buzz_done_ = cpu_valid_ = stop_one_page_ = stop_full_ = false;
    clock_->Disarm(event_);
    ints_.Reset();
}

void Vr4101Aiu::ApplyClock() {
    CatchUp();
    ticks_.SetCounting(gate_.Running());
    Rearm();
}

void Vr4101Aiu::SaveState(StateWriter& w) {
    w.Write("mode", mode_);
    w.Write("seqen", seqen_);
    w.Write("mute", mute_);
    w.Write("stppage", stppage_);
    w.Write("repnum", rep_);
    w.Write("busen", busen_);
    w.Write("count0", count0_);
    w.Write("count1", count1_);
    ints_.Save(w);
    w.Write("pair_high", pair_high_);
    w.Write("pair_low", pair_low_);
    w.Write("segment", seg_);
    w.Write("cursor", cursor_);
    w.Write("seg_start", seg_start_);
    w.Write("seg_end", seg_end_);
    w.Write("buzz_half", half_);
    w.Write<uint8_t>("primed", primed_ ? 1u : 0u);
    w.Write<uint8_t>("idle", idle_ ? 1u : 0u);
    w.Write<uint8_t>("buzz_done", buzz_done_ ? 1u : 0u);
    w.Write<uint8_t>("cpu_valid", cpu_valid_ ? 1u : 0u);
    w.Write<uint8_t>("stop_one_page", stop_one_page_ ? 1u : 0u);
    w.Write<uint8_t>("stop_full", stop_full_ ? 1u : 0u);
    ticks_.Save(w, "aiu_");
    out_.Save(w);
}

void Vr4101Aiu::RestoreState(StateReader& r) {
    uint8_t primed = 0u, idle = 0u, buzz_done = 0u, cpu_valid = 0u, stop_one = 0u, stop_full = 0u;
    r.Read("mode", mode_);
    r.Read("seqen", seqen_);
    r.Read("mute", mute_);
    r.Read("stppage", stppage_);
    r.Read("repnum", rep_);
    r.Read("busen", busen_);
    r.Read("count0", count0_);
    r.Read("count1", count1_);
    ints_.Restore(r);
    r.Read("pair_high", pair_high_);
    r.Read("pair_low", pair_low_);
    r.Read("segment", seg_);
    r.Read("cursor", cursor_);
    r.Read("seg_start", seg_start_);
    r.Read("seg_end", seg_end_);
    r.Read("buzz_half", half_);
    r.Read("primed", primed);
    r.Read("idle", idle);
    r.Read("buzz_done", buzz_done);
    r.Read("cpu_valid", cpu_valid);
    r.Read("stop_one_page", stop_one);
    r.Read("stop_full", stop_full);
    ticks_.Restore(r, "aiu_");
    out_.Restore(r);
    primed_        = primed != 0u;
    idle_          = idle != 0u;
    buzz_done_     = buzz_done != 0u;
    cpu_valid_     = cpu_valid != 0u;
    stop_one_page_ = stop_one != 0u;
    stop_full_     = stop_full != 0u;
    clock_->Disarm(event_);
}

void Vr4101Aiu::PostRestore() {
    out_.Resume(seqen_ != 0u);
    if (seqen_) flush_.Start(kHostPcmFlushRate);
    else        flush_.Stop();
    gate_.Refresh();
    ints_.Publish();
}

}

REGISTER_SERVICE_AS(Vr4101Aiu, Vr41xxAiuDma);

}

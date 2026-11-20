#include "vr41xx_aiu_dma.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/boards/board_context.h>
#include "../../execution/guest_cycle_clock.h"
#include "../vr4102/vr4102_id.h"
#include "../vr4111/vr4111_id.h"
#include "../vr4121/vr4121_id.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/state/hibernation.h>
#include <cerfcore/state/state_stream.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "vr41xx_aiu_interrupts.h"
#include "vr41xx_aiu_mic.h"
#include "vr41xx_aiu_speaker.h"
#include "vr41xx_audio_switch.h"
#include "vr41xx_cmu.h"
#include "vr41xx_unit_clock_gate.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x0B000160u;
constexpr uint32_t kSize = 0x20u;

constexpr uint32_t kOffMdmadat = 0x00u;
constexpr uint32_t kOffSdmadat = 0x02u;
constexpr uint32_t kOffSodat   = 0x06u;
constexpr uint32_t kOffScnt    = 0x08u;
constexpr uint32_t kOffScnvr   = 0x0Au;
constexpr uint32_t kOffMidat   = 0x10u;
constexpr uint32_t kOffMcnt    = 0x12u;
constexpr uint32_t kOffMcnvr   = 0x14u;
constexpr uint32_t kOffDvalid  = 0x18u;
constexpr uint32_t kOffSeq     = 0x1Au;
constexpr uint32_t kOffInt     = 0x1Cu;

constexpr uint16_t kSeqAiurst    = 0x8000u;
constexpr uint16_t kSeqAiumen    = 0x0010u;
constexpr uint16_t kSeqAiusen    = 0x0001u;
constexpr uint16_t kCntEnable    = 0x8000u;
constexpr uint16_t kCntStopOpen  = 0x0002u;
constexpr uint16_t kCntWritable  = 0x8002u;
constexpr uint16_t kCntState     = 0x0008u;
constexpr uint16_t kRateMask     = 0x0007u;
constexpr uint16_t kIntCauseMask = 0x0F0Eu;
constexpr uint16_t kDrqAout      = 0x0004u;
constexpr uint16_t kDrqAin       = 0x0008u;

class Vr41xxAiu : public Vr41xxAiuDma, private Vr41xxAiuPathHost {
public:
    explicit Vr41xxAiu(CerfEmulator& emu) : Vr41xxAiuDma(emu), speaker_(emu, *this), mic_(emu, *this) {}

    bool ShouldRegister() override {
        const std::string_view soc = emu_.Get<BoardContext>().GetSocId();
        return soc == SocId::Vr4102 || soc == SocId::Vr4111 || soc == SocId::Vr4121;
    }

    void OnReady() override;
    void OnShutdown() override {
        speaker_.StopHost();
        mic_.StopHost();
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    uint32_t ReadWord  (uint32_t addr) override { return ReadWordAsHalves(addr); }
    uint64_t ReadDword (uint32_t addr) override { return ReadDwordAsHalves(addr); }
    void     WriteWord (uint32_t addr, uint32_t v) override { WriteWordAsHalves(addr, v); }
    void     WriteDword(uint32_t addr, uint64_t v) override { WriteDwordAsHalves(addr, v); }

    void SyncDma() override {
        speaker_.CatchUp();
        mic_.CatchUp();
    }
    uint16_t DmaRequests() override {
        return static_cast<uint16_t>((speaker_.RequestPending() ? kDrqAout : 0u) |
                                     (mic_.RequestPending() ? kDrqAin : 0u));
    }

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

private:
    void DmaRegistersWritten() override {
        speaker_.Settle();
        mic_.Settle();
    }
    void RaiseAiuInt(uint16_t bits) override { ints_.Raise(bits); }
    bool AiuIntPending(uint16_t bits) const override { return ints_.Pending(bits); }
    bool StopAfterOnePage(Vr41xxAiuChannel channel) const override { return (CntReg(channel) & kCntStopOpen) != 0u; }
    bool ConverterEnabled(Vr41xxAiuChannel channel) const override { return (CntReg(channel) & kCntEnable) != 0u; }
    uint16_t CntReg(Vr41xxAiuChannel channel) const { return channel == Vr41xxAiuChannel::Out ? scnt_ : mcnt_; }

    void WriteRegister(uint32_t addr, uint16_t value);
    void WriteSeq(uint16_t value);
    void ClearInt(uint16_t value);
    void ResetUnit();
    void ApplyClock();

    Vr41xxAiuSpeaker    speaker_;
    Vr41xxAiuMic        mic_;
    Vr41xxAiuInterrupts ints_{emu_};
    Vr41xxUnitClockGate gate_{emu_};
    uint16_t            scnt_ = 0u;
    uint16_t            mcnt_ = 0u;
};

void Vr41xxAiu::OnReady() {
    emu_.Get<PeripheralDispatcher>().Register(this);
    ints_.Attach();
    speaker_.AttachHost();
    mic_.AttachHost();

    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { ResetUnit(); });
    gate_.Attach(kCmuMskAiu, [this] { ApplyClock(); });
    emu_.Get<GuestCycleClock>().RegisterRateListener([this] {
        speaker_.RescaleClock();
        mic_.RescaleClock();
    });
    emu_.Get<Vr41xxAudioSwitch>().RegisterChangeListener([this] {
        speaker_.SwitchChanged();
        mic_.SwitchChanged();
    });
    emu_.Get<Hibernation>().RegisterPreSaveListener([this] { SyncDma(); });
    gate_.Refresh();
}

uint16_t Vr41xxAiu::ReadHalf(uint32_t addr) {
    SyncDma();
    switch (addr - kBase) {
        case kOffMdmadat: return mic_.MdmadatReg();
        case kOffSdmadat: return speaker_.SdmadatReg();
        case kOffSodat:   return speaker_.SodatReg();
        case kOffScnt:    return static_cast<uint16_t>(scnt_ | (speaker_.Enabled() ? kCntState : 0u));
        case kOffScnvr:   return speaker_.RateCode();
        case kOffMidat:   return mic_.MidatReg();
        case kOffMcnt:    return static_cast<uint16_t>(mcnt_ | (mic_.Enabled() ? kCntState : 0u));
        case kOffMcnvr:   return mic_.RateCode();
        case kOffDvalid:  return static_cast<uint16_t>(speaker_.ValidBits() | mic_.ValidBits());
        case kOffSeq:
            return static_cast<uint16_t>((mic_.Enabled() ? kSeqAiumen : 0u) | (speaker_.Enabled() ? kSeqAiusen : 0u));
        case kOffInt:     return ints_.Bits();
        default: HaltUnsupportedAccess("AIU ReadHalf", addr, 0);
    }
}

void Vr41xxAiu::WriteHalf(uint32_t addr, uint16_t value) {
    WriteDmaRegister([&] { WriteRegister(addr, value); });
}

void Vr41xxAiu::WriteRegister(uint32_t addr, uint16_t value) {
    switch (addr - kBase) {
        case kOffMdmadat: if (mic_.Enabled()) mic_.WriteMdmadat(value); break;
        case kOffSdmadat: if (speaker_.Enabled()) speaker_.WriteSdmadat(value); break;
        case kOffSodat:   if (speaker_.Enabled()) speaker_.WriteSodat(value); break;
        case kOffScnt:    scnt_ = static_cast<uint16_t>(value & kCntWritable); break;
        case kOffScnvr:   speaker_.SetRateCode(static_cast<uint16_t>(value & kRateMask)); break;
        case kOffMidat:   if (mic_.Enabled()) mic_.WriteMidat(value); break;
        case kOffMcnt:    mcnt_ = static_cast<uint16_t>(value & kCntWritable); break;
        case kOffMcnvr:   mic_.SetRateCode(static_cast<uint16_t>(value & kRateMask)); break;
        case kOffDvalid:
            if (speaker_.Enabled()) speaker_.WriteValidBits(value);
            if (mic_.Enabled()) mic_.WriteValidBits(value);
            break;
        case kOffSeq:     WriteSeq(value); break;
        case kOffInt:     ClearInt(value); break;
        default: HaltUnsupportedAccess("AIU WriteHalf", addr, value);
    }
}

void Vr41xxAiu::WriteSeq(uint16_t value) {
    if (value & kSeqAiurst) {
        if (value & (kSeqAiusen | kSeqAiumen)) {
            emu_.Get<Fatal>().Die("Vr41xxAiu: SEQREG write 0x%04X sets AIURST together with an enable bit", value);
        }
        ResetUnit();
        return;
    }
    speaker_.SetEnabled((value & kSeqAiusen) != 0u);
    mic_.SetEnabled((value & kSeqAiumen) != 0u);
}

void Vr41xxAiu::ClearInt(uint16_t value) {
    const uint16_t cleared = ints_.Clear(static_cast<uint16_t>(value & kIntCauseMask));
    speaker_.ReleaseStops(cleared);
    mic_.ReleaseStops(cleared);
}

void Vr41xxAiu::ResetUnit() {
    speaker_.Reset();
    mic_.Reset();
    scnt_ = 0u;
    mcnt_ = 0u;
    ints_.Reset();
}

void Vr41xxAiu::ApplyClock() {
    const bool running = gate_.Running();
    speaker_.SetClockRunning(running);
    mic_.SetClockRunning(running);
}

void Vr41xxAiu::SaveState(StateWriter& w) {
    w.Write("scnt", scnt_);
    w.Write("mcnt", mcnt_);
    ints_.Save(w);
    gate_.Save(w);
    speaker_.Save(w);
    mic_.Save(w);
}

void Vr41xxAiu::RestoreState(StateReader& r) {
    r.Read("scnt", scnt_);
    r.Read("mcnt", mcnt_);
    ints_.Restore(r);
    gate_.Restore(r);
    speaker_.Restore(r);
    mic_.Restore(r);
}

void Vr41xxAiu::PostRestore() {
    speaker_.PostRestore();
    mic_.PostRestore();
    gate_.Refresh();
    ints_.Publish();
}

}

REGISTER_SERVICE_AS(Vr41xxAiu, Vr41xxAiuDma);

}

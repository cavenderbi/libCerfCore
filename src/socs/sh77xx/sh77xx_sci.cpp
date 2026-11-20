#include "sh77xx_sci.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../execution/guest_cycle_clock.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../tracing/kernel_debug_sink.h"
#include "sh77xx_cpg.h"
#include "sh77xx_frqcr.h"
#include "sh77xx_intc.h"
#include "sh77xx_sci_channel.h"
#include "sh77xx_sci_port.h"
#include "sh77xx_serial_format.h"
#include "sh77xx_serial_wiring.h"
#include "sh77xx_standby_control.h"
#include "sh77xx_standby_entry.h"

#include <optional>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kOffMode     = 0x0u;
constexpr uint32_t kOffBitRate  = 0x2u;
constexpr uint32_t kOffControl  = 0x4u;
constexpr uint32_t kOffTransmit = 0x6u;
constexpr uint32_t kOffStatus   = 0x8u;
constexpr uint32_t kOffReceive  = 0xAu;
constexpr uint32_t kWindowBytes = kOffReceive + 1u;

constexpr uint8_t kModeSync           = 0x80u;
constexpr uint8_t kModeMultiprocessor = 0x04u;

constexpr uint8_t kCtlTie       = 0x80u;
constexpr uint8_t kCtlRie       = 0x40u;
constexpr uint8_t kCtlTe        = 0x20u;
constexpr uint8_t kCtlRe        = 0x10u;
constexpr uint8_t kCtlTeie      = 0x04u;
constexpr uint8_t kCtlUnmodeled = 0x0Bu;

constexpr uint8_t kStFer       = 0x10u;
constexpr uint8_t kStPer       = 0x08u;
constexpr uint8_t kStMpbt      = 0x01u;
constexpr uint8_t kStReceive   = kSh77xxSciRdrf | kSh77xxSciOrer;
constexpr uint8_t kStClearable = kSh77xxSciTdre | kStReceive | kStFer | kStPer;

constexpr uint8_t kMstp0 = 0x01u;

constexpr uint8_t kInitialMode    = 0x00u;
constexpr uint8_t kInitialBitRate = 0xFFu;
constexpr uint8_t kInitialControl = 0x00u;

constexpr char kSource[] = "SCI";

}

Sh77xxSci::Sh77xxSci(CerfEmulator& emu)
    : Peripheral(emu),
      rx_line_(emu),
      tx_line_(emu),
      endpoint_timer_(emu, [this] { port_.OnTimer(); }),
      port_(*this, endpoint_timer_, [this] { return ControlLines(); }) {}

void Sh77xxSci::OnReady() {
    intc_   = &emu_.Get<Sh77xxIntc>();
    stbcr_  = &emu_.Get<Sh77xxStbcr>();
    cpg_    = &emu_.Get<Sh77xxCpg>();
    frqcr_  = &emu_.Get<Sh77xxFrqcr>();
    wiring_ = emu_.TryGet<Sh77xxSerialWiring>();
    scsptr_ = &emu_.Get<Sh77xxSciPort>();
    rx_line_.Attach([this] { OnLineDue(); }, [this] { OnHostRequest(); },
                    [this] {
                        std::lock_guard<std::mutex> lk(mu_);
                        rx_line_.Rescale();
                    });
    tx_line_.Attach([this] { OnLineDue(); });
    scsptr_->AddChangeListener([this] { port_.NotifyControlLines(); });
    emu_.Get<GuestCycleClock>().RegisterRateListener([this] {
        std::lock_guard<std::mutex> lk(mu_);
        Check(tx_line_.Rescale());
    });
    endpoint_timer_.AttachLineTimer();
    stbcr_->RegisterChangeListener([this](uint8_t from, uint8_t to) {
        if ((from & kMstp0) == 0u && (to & kMstp0) != 0u) InitializeAndNotify();
        RefreshClockStop();
    });
    emu_.Get<Sh77xxStandbyEntry>().RegisterListener([this] { InitializeAndNotify(); });
    GuestCpuReset& reset = emu_.Get<GuestCpuReset>();
    reset.RegisterResetListener([this](ResetLineKind) { Initialize(); });
    reset.RegisterResetReleaseListener([this] {
        RefreshClockStop();
        RefreshConnection();
        port_.NotifyControlLines();
    });
    frqcr_->RegisterChangeListener([this](uint16_t, uint16_t) { Reformat(smr_); });
    Initialize();
    RefreshClockStop();
    RefreshConnection();
    emu_.Get<PeripheralDispatcher>().Register(this);
    if (wiring_ == nullptr) return;
    wiring_->SetControlListener(kSh77xxSciBase, [this] { OnWiringChanged(); });
    std::optional<ObjectIdentity> cradle = wiring_->Cradle(kSh77xxSciBase);
    if (cradle) {
        port_.AttachCradle(emu_, std::move(*cradle));
        return;
    }
    port_.SetActivityObject(wiring_->ShareLine(kSh77xxSciBase, *this));
}

void Sh77xxSci::OnShutdown() {
    SettleAndDeliver();
    port_.OnShutdown();
    emu_.Get<KernelDebugSink>().FlushLine(debug_text_, kSource);
}

uint32_t Sh77xxSci::MmioBase() const { return kSh77xxSciBase; }
uint32_t Sh77xxSci::MmioSize() const { return kWindowBytes; }

uint8_t Sh77xxSci::ReadByte(uint32_t addr) {
    RequireRunning("ReadByte", addr, 0u);
    if (addr - kSh77xxSciBase == kOffReceive) return ReadReceive();
    uint8_t              value = 0u;
    std::vector<uint8_t> out;
    {
        std::lock_guard<std::mutex> lk(mu_);
        switch (addr - kSh77xxSciBase) {
        case kOffMode:
            value = smr_;
            break;
        case kOffBitRate:
            value = brr_;
            break;
        case kOffControl:
            value = scr_;
            break;
        case kOffTransmit:
            value = tx_line_.Data();
            break;
        case kOffStatus:
            SettleLocked();
            value       = StatusLocked();
            read_flags_ = static_cast<uint8_t>(read_flags_ | (value & kStClearable));
            break;
        default:
            HaltUnsupportedAccess("ReadByte", addr, 0u);
        }
        tx_line_.TakeSent(out);
    }
    Deliver(out);
    return value;
}

void Sh77xxSci::WriteByte(uint32_t addr, uint8_t value) {
    RequireRunning("WriteByte", addr, value);
    switch (addr - kSh77xxSciBase) {
    case kOffMode:
        if ((value & (kModeSync | kModeMultiprocessor)) != 0u) HaltUnsupportedAccess("WriteByte", addr, value);
        Reformat(value);
        return;
    case kOffBitRate:
        {
            std::lock_guard<std::mutex> lk(mu_);
            brr_ = value;
        }
        Reformat(smr_);
        return;
    case kOffControl:
        WriteControl(addr, value);
        return;
    case kOffTransmit:
        WriteTransmit(value);
        return;
    case kOffStatus:
        WriteStatus(value);
        return;
    default:
        HaltUnsupportedAccess("WriteByte", addr, value);
    }
}

void Sh77xxSci::PushRx(const uint8_t* data, size_t n) {
    if (n == 0u) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (stopped_ || !powered_ || (scr_ & kCtlRe) == 0u || !rx_line_.Push(data, n)) return;
    }
    port_.MarkRx();
}

bool Sh77xxSci::RxEmpty() const {
    std::lock_guard<std::mutex> lk(mu_);
    return rx_line_.Empty();
}

void Sh77xxSci::SetRxDrainCallback(RxDrainFn cb) { port_.SetRxDrainCallback(std::move(cb)); }

void Sh77xxSci::SetModemInputs(bool cts, bool dsr, bool ri, bool dcd) {
    wiring_->SetModemInputs(kSh77xxSciBase, cts, dsr, ri, dcd);
}

SerialLine::LineConfig Sh77xxSci::GetLineConfig() const {
    std::lock_guard<std::mutex> lk(mu_);
    return line_cfg_;
}

void Sh77xxSci::SetLineConfigCallback(LineConfigFn cb) { port_.SetLineConfigCallback(std::move(cb)); }
void Sh77xxSci::SetEndpoint(SerialEndpoint* ep) { port_.SetEndpoint(ep); }
void Sh77xxSci::ArmEndpointTimer(int64_t delay_ns) { port_.ArmTimer(delay_ns); }
void Sh77xxSci::DisarmEndpointTimer() { port_.DisarmTimer(); }

void Sh77xxSci::SaveState(StateWriter& w) {
    port_.SaveCradleState(w);
    {
        std::lock_guard<std::mutex> lk(mu_);
        w.Write("smr", smr_);
        w.Write("brr", brr_);
        w.Write("scr", scr_);
        w.Write("mpbt", mpbt_);
        w.Write("read_flags", read_flags_);
        rx_line_.Save(w);
        tx_line_.Save(w);
    }
    endpoint_timer_.SaveLineTimer(w, "endpoint_timer_");
    port_.SaveEndpointState(w);
}

void Sh77xxSci::RestoreState(StateReader& r) {
    emu_.Get<KernelDebugSink>().FlushLine(debug_text_, kSource);
    port_.RestoreCradleState(r);
    {
        std::lock_guard<std::mutex> lk(mu_);
        r.Read("smr", smr_);
        r.Read("brr", brr_);
        r.Read("scr", scr_);
        r.Read("mpbt", mpbt_);
        r.Read("read_flags", read_flags_);
        rx_line_.Restore(r);
        Check(tx_line_.Restore(r));
    }
    endpoint_timer_.RestoreLineTimer(r, "endpoint_timer_");
    port_.RestoreEndpointState(r);
}

void Sh77xxSci::PostRestore() {
    endpoint_timer_.ResumeLineTimer();
    RefreshClockStop();
    RefreshConnection();
    {
        std::lock_guard<std::mutex> lk(mu_);
        rx_line_.SetArmed((scr_ & kCtlRie) != 0u);
        tx_line_.SetInterrupts((scr_ & kCtlTie) != 0u, (scr_ & kCtlTeie) != 0u);
        DriveLocked();
    }
    scsptr_->SetTransmitter(Transmitting());
    FireLineConfig();
    port_.PostRestore();
}

void Sh77xxSci::RequireRunning(const char* access, uint32_t addr, uint32_t value) {
    if (stbcr_->ModuleStopped(kMstp0)) HaltUnsupportedAccess(access, addr, value);
}

bool Sh77xxSci::Transmitting() const { return (scr_ & kCtlTe) != 0u; }

SerialControlLines Sh77xxSci::ControlLines() {
    bool powered = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        powered = powered_;
    }
    SerialControlLines lines = wiring_->Control(kSh77xxSciBase);
    lines.in_break           = powered && scsptr_->TxdPinLow();
    return lines;
}

uint8_t Sh77xxSci::StatusLocked() const { return static_cast<uint8_t>(tx_line_.Flags() | rx_line_.Flags() | mpbt_); }

void Sh77xxSci::Reformat(uint8_t smr) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        rx_line_.Reformat(smr_, smr, brr_);
        Check(tx_line_.Reformat(smr, brr_));
        smr_ = smr;
    }
    FireLineConfig();
}

void Sh77xxSci::WriteControl(uint32_t addr, uint8_t value) {
    if ((value & kCtlUnmodeled) != 0u) HaltUnsupportedAccess("WriteByte", addr, value);
    bool                 drained      = false;
    bool                 te_changed   = false;
    bool                 transmitting = false;
    std::vector<uint8_t> out;
    {
        std::lock_guard<std::mutex> lk(mu_);
        SettleLocked();
        const uint8_t was = scr_;
        scr_              = value;
        te_changed        = ((was ^ value) & kCtlTe) != 0u;
        transmitting      = Transmitting();
        if ((was & kCtlTe) != 0u && !Transmitting()) ForgetReadsLocked(tx_line_.Disable());
        if ((was & kCtlRe) != 0u && (value & kCtlRe) == 0u) {
            const bool had = !rx_line_.Empty();
            rx_line_.Stop();
            drained = had && rx_line_.Empty();
        }
        rx_line_.SetArmed((value & kCtlRie) != 0u);
        tx_line_.SetInterrupts((value & kCtlTie) != 0u, (value & kCtlTeie) != 0u);
        DriveLocked();
        tx_line_.TakeSent(out);
    }
    Deliver(out);
    if (drained) port_.NotifyRxDrained();
    if (!te_changed) return;
    scsptr_->SetTransmitter(transmitting);
    port_.NotifyControlLines();
}

void Sh77xxSci::WriteTransmit(uint8_t value) {
    std::vector<uint8_t> out;
    {
        std::lock_guard<std::mutex> lk(mu_);
        SettleLocked();
        tx_line_.Write(value);
        tx_line_.TakeSent(out);
    }
    Deliver(out);
}

void Sh77xxSci::WriteStatus(uint8_t value) {
    bool                 drained = false;
    std::vector<uint8_t> out;
    {
        std::lock_guard<std::mutex> lk(mu_);
        SettleLocked();
        const uint8_t clears = static_cast<uint8_t>(~value & StatusLocked() & read_flags_ & kStClearable);
        mpbt_                = static_cast<uint8_t>(value & kStMpbt);
        if ((clears & kSh77xxSciTdre) != 0u) Check(tx_line_.ClearEmpty(smr_, brr_, Transmitting()));
        rx_line_.Clear(static_cast<uint8_t>(clears & kStReceive));
        read_flags_ = static_cast<uint8_t>(read_flags_ & ~clears);
        drained     = (clears & kSh77xxSciRdrf) != 0u && rx_line_.Empty();
        DriveLocked();
        tx_line_.TakeSent(out);
    }
    Deliver(out);
    if (drained) port_.NotifyRxDrained();
}

uint8_t Sh77xxSci::ReadReceive() {
    uint8_t              value = 0u;
    std::vector<uint8_t> out;
    {
        std::lock_guard<std::mutex> lk(mu_);
        SettleLocked();
        value = rx_line_.Data();
        tx_line_.TakeSent(out);
    }
    Deliver(out);
    return value;
}

void Sh77xxSci::Deliver(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return;
    bool powered = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        powered = powered_;
    }
    emu_.Get<KernelDebugSink>().DeliverOrEmit(port_, bytes.data(), bytes.size(), powered, debug_text_, kSource);
}

void Sh77xxSci::OnHostRequest() {
    std::lock_guard<std::mutex> lk(mu_);
    rx_line_.TakeWire(smr_, brr_, (scr_ & kCtlRe) != 0u);
}

void Sh77xxSci::SettleLocked() {
    const uint8_t risen = static_cast<uint8_t>(rx_line_.Settle() | tx_line_.Settle());
    if (risen == 0u) return;
    ForgetReadsLocked(risen);
    DriveLocked();
}

void Sh77xxSci::ForgetReadsLocked(uint8_t risen) { read_flags_ = static_cast<uint8_t>(read_flags_ & ~risen); }

void Sh77xxSci::OnLineDue() { SettleAndDeliver(); }

void Sh77xxSci::SettleAndDeliver() {
    std::vector<uint8_t> out;
    {
        std::lock_guard<std::mutex> lk(mu_);
        SettleLocked();
        tx_line_.TakeSent(out);
    }
    Deliver(out);
}

void Sh77xxSci::DriveLocked() {
    const uint8_t ssr = StatusLocked();
    intc_->SetSourceRequest(Sh77xxIntcSource::kTxi, (scr_ & kCtlTie) != 0u && (ssr & kSh77xxSciTdre) != 0u);
    intc_->SetSourceRequest(Sh77xxIntcSource::kRxi, (scr_ & kCtlRie) != 0u && (ssr & kSh77xxSciRdrf) != 0u);
    intc_->SetSourceRequest(Sh77xxIntcSource::kEri, (scr_ & kCtlRie) != 0u && (ssr & kSh77xxSciOrer) != 0u);
    intc_->SetSourceRequest(Sh77xxIntcSource::kTei, (scr_ & kCtlTeie) != 0u && (ssr & kSh77xxSciTend) != 0u);
}

SerialLine::LineConfig Sh77xxSci::LineConfigLocked() const {
    LineConfig c = Sh77xxSerialFrameFormat(smr_);
    c.baud       = Sh77xxSerialBaud(smr_, brr_, cpg_->PeripheralRate(frqcr_->Value()));
    return c;
}

void Sh77xxSci::FireLineConfig() {
    LineConfig cfg;
    {
        std::lock_guard<std::mutex> lk(mu_);
        cfg       = LineConfigLocked();
        line_cfg_ = cfg;
    }
    port_.NotifyLineConfig(cfg);
}

void Sh77xxSci::Initialize() {
    SettleAndDeliver();
    bool drained = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        drained     = !rx_line_.Empty();
        smr_        = kInitialMode;
        brr_        = kInitialBitRate;
        scr_        = kInitialControl;
        mpbt_       = 0u;
        read_flags_ = 0u;
        rx_line_.Initialize();
        tx_line_.Initialize();
        DriveLocked();
    }
    scsptr_->SetTransmitter(Transmitting());
    if (drained) port_.NotifyRxDrained();
    FireLineConfig();
}

void Sh77xxSci::InitializeAndNotify() {
    Initialize();
    port_.NotifyControlLines();
}

void Sh77xxSci::RefreshClockStop() {
    const bool stopped = stbcr_->ModuleStopped(kMstp0);
    std::lock_guard<std::mutex> lk(mu_);
    stopped_ = stopped;
}

void Sh77xxSci::RefreshConnection() {
    const bool powered = wiring_ != nullptr && wiring_->DriverPowered(kSh77xxSciBase);
    std::lock_guard<std::mutex> lk(mu_);
    powered_ = powered;
}

void Sh77xxSci::OnWiringChanged() {
    SettleAndDeliver();
    RefreshConnection();
    port_.NotifyControlLines();
}

void Sh77xxSci::Check(bool scaled) {
    if (!scaled) emu_.Get<Fatal>().Die("Sh77xxSci: the bit rate overflows the guest clock scale");
}

}

#include "sh77xx_scif.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../core/deferred_jobs.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../tracing/kernel_debug_sink.h"
#include "sh77xx_cpg.h"
#include "sh77xx_frqcr.h"
#include "sh77xx_scif_external_clock.h"
#include "sh77xx_scif_pins.h"
#include "sh77xx_serial_format.h"
#include "sh77xx_serial_wiring.h"
#include "sh77xx_standby_entry.h"

#include <optional>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kOffMode     = 0x0u;
constexpr uint32_t kOffBitRate  = 0x2u;
constexpr uint32_t kOffControl  = 0x4u;
constexpr uint32_t kOffStatus   = 0x8u;
constexpr uint32_t kOffFifoCtl  = 0xCu;
constexpr uint32_t kOffFifoData = 0xEu;

constexpr uint8_t kModeChr      = 0x40u;
constexpr uint8_t kModeIrda     = 0x80u;
constexpr uint8_t kModeWritable = 0x7Bu;

constexpr uint8_t kCtlTie       = 0x80u;
constexpr uint8_t kCtlRie       = 0x40u;
constexpr uint8_t kCtlTe        = 0x20u;
constexpr uint8_t kCtlRe        = 0x10u;
constexpr uint8_t kCtlReserved  = 0x0Cu;
constexpr uint8_t kCtlClockBits = 0x03u;
constexpr uint8_t kCtlCke1      = 0x02u;
constexpr uint8_t kCtlClockOut  = 0x01u;

constexpr uint32_t kExternalClocksPerBit = 16u;

constexpr uint8_t kFifoTfrst = 0x04u;
constexpr uint8_t kFifoRfrst = 0x02u;
constexpr uint8_t kFifoLoop  = 0x01u;

}

Sh77xxScif::Sh77xxScif(CerfEmulator& emu)
    : Peripheral(emu),
      endpoint_timer_(emu, [this] { port_.OnTimer(); }),
      port_(*this, endpoint_timer_, [this] { return ControlLines(); }) {}

Sh77xxScif::~Sh77xxScif() = default;

void Sh77xxScif::OnReady() {
    intc_   = &emu_.Get<Sh77xxIntc>();
    pins_   = &emu_.Get<Sh77xxScifPins>();
    stbcr2_ = emu_.TryGet<Sh77xxStbcr2>();
    cpg_    = &emu_.Get<Sh77xxCpg>();
    frqcr_  = &emu_.Get<Sh77xxFrqcr>();
    wiring_ = emu_.TryGet<Sh77xxSerialWiring>();
    clock_  = emu_.TryGet<Sh77xxScifExternalClock>();
    jobs_   = &emu_.Get<DeferredJobs>();
    endpoint_timer_.AttachLineTimer();
    if (clock_ != nullptr) clock_->AddChangeListener([this] { FireLineConfig(); });

    if (stbcr2_ != nullptr) {
        stbcr2_->RegisterChangeListener([this](uint8_t from, uint8_t to) {
            const bool stopping = (from & Channel().mstp) == 0u && (to & Channel().mstp) != 0u;
            if (stopping) EnterStandby();
            RefreshClockStop();
            if (stopping) FireLineConfig();
        });
    }
    frqcr_->RegisterChangeListener([this](uint16_t, uint16_t) { FireLineConfig(); });
    emu_.Get<Sh77xxStandbyEntry>().RegisterListener([this] {
        EnterStandby();
        FireLineConfig();
    });
    GuestCpuReset& reset = emu_.Get<GuestCpuReset>();
    reset.RegisterResetListener([this](ResetLineKind) { Initialize(); });
    reset.RegisterResetReleaseListener([this] { Resync(); });

    Initialize();
    PublishEnables();
    modem_.Attach(*pins_, *jobs_, Channel(), [this] { return RtsHigh(); });
    RefreshClockStop();
    RefreshConnection();
    {
        std::lock_guard<std::mutex> lk(mu_);
        line_cfg_ = LineConfigLocked();
    }
    emu_.Get<PeripheralDispatcher>().Register(this);

    if (wiring_ == nullptr) return;
    std::optional<ObjectIdentity> cradle = wiring_->Cradle(Channel().base);
    MachineObject*                shared = cradle ? nullptr : wiring_->ShareLine(Channel().base, *this);
    if (!cradle && shared == nullptr) return;
    wiring_->SetControlListener(Channel().base, [this] {
        RefreshConnection();
        port_.NotifyControlLines();
    });
    RefreshTxLine();
    pins_->AddPinListener([this] {
        RefreshTxLine();
        port_.NotifyControlLines();
        PostRelease();
    });
    if (cradle) port_.AttachCradle(emu_, std::move(*cradle));
    if (shared != nullptr) port_.SetActivityObject(shared);
}

void Sh77xxScif::OnShutdown() {
    port_.OnShutdown();
    emu_.Get<KernelDebugSink>().FlushLine(tx_line_, Channel().name);
}

SerialControlLines Sh77xxScif::ControlLines() {
    SerialControlLines lines   = wiring_->Control(Channel().base);
    bool               powered = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        powered = powered_;
    }
    lines.in_break = powered && tx_low_.load(std::memory_order_acquire);
    return lines;
}

void Sh77xxScif::RefreshTxLine() {
    tx_low_.store(pins_->TxdLow(Channel()), std::memory_order_release);
}

void Sh77xxScif::Resync() {
    RefreshClockStop();
    RefreshConnection();
    PublishEnables();
    RefreshTxLine();
    modem_.RefreshRts();
    FireLineConfig();
}

void Sh77xxScif::RequireRunning(const char* reg, uint32_t value, uint32_t held) const {
    if (!stopped_ || value == held) return;
    emu_.Get<Fatal>().Die("Sh77xxScif: %s write 0x%X at 0x%08X while the module clock is stopped", reg, value,
                          Channel().base);
}

uint8_t Sh77xxScif::ReadByte(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (off == kSh77xxScifReceiveOffset) return ReadReceiveData();
    std::lock_guard<std::mutex> lk(mu_);
    switch (off) {
        case kOffMode:    return smr_;
        case kOffBitRate: return brr_;
        case kOffControl: return scr_;
        case kOffFifoCtl: return fcr_;
        default:          HaltUnsupportedAccess("ReadByte", addr, 0u);
    }
}

void Sh77xxScif::WriteByte(uint32_t addr, uint8_t value) {
    switch (addr - MmioBase()) {
        case kOffMode:     WriteMode(value); return;
        case kOffBitRate:  WriteBitRate(value); return;
        case kOffControl:  WriteControl(value); return;
        case kSh77xxScifTransmitOffset: WriteTransmitData(value); return;
        case kOffFifoCtl:  WriteFifoControl(value); return;
        default:           HaltUnsupportedAccess("WriteByte", addr, value);
    }
}

uint16_t Sh77xxScif::ReadHalf(uint32_t addr) {
    switch (addr - MmioBase()) {
        case kOffStatus: return ReadStatus();
        case kOffFifoData: {
            std::lock_guard<std::mutex> lk(mu_);
            return fifo_.DataCounts();
        }
        default: HaltUnsupportedAccess("ReadHalf", addr, 0u);
    }
}

void Sh77xxScif::WriteHalf(uint32_t addr, uint16_t value) {
    if (addr - MmioBase() != kOffStatus) HaltUnsupportedAccess("WriteHalf", addr, value);
    WriteStatus(value);
}

void Sh77xxScif::WriteMode(uint8_t value) {
    const uint8_t stored = static_cast<uint8_t>(value & (Channel().irda ? 0xFFu : kModeWritable));
    {
        std::lock_guard<std::mutex> lk(mu_);
        RequireRunning("SCSMR", stored, smr_);
        smr_ = stored;
    }
    FireLineConfig();
}

void Sh77xxScif::WriteBitRate(uint8_t value) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        RequireRunning("SCBRR", value, brr_);
        brr_ = value;
    }
    FireLineConfig();
}

void Sh77xxScif::WriteControl(uint8_t value) {
    const uint8_t clock_bits = static_cast<uint8_t>(value & kCtlClockBits);
    if ((value & kCtlReserved) != 0u || clock_bits == kCtlClockOut ||
        ((value & kCtlCke1) != 0u && clock_ == nullptr)) {
        emu_.Get<Fatal>().Die("Sh77xxScif: SCSCR write 0x%02X at 0x%08X", value, Channel().base);
    }
    std::vector<uint8_t> out;
    bool                 powered       = false;
    bool                 clock_changed = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        RequireRunning("SCSCR", value, scr_);
        if ((scr_ & kCtlTe) != 0u && (value & kCtlTe) == 0u) fifo_.EndTransmit();
        clock_changed = ((scr_ ^ value) & kCtlCke1) != 0u;
        scr_          = value;
        TakeTransmitLocked(out);
        powered = powered_;
        DriveLocked();
    }
    PublishEnables();
    if (clock_changed) FireLineConfig();
    DeliverTx(out, powered);
}

void Sh77xxScif::WriteFifoControl(uint8_t value) {
    if ((value & kFifoLoop) != 0u || (Sh77xxScifModemLines::Enabled(value) && !Channel().modem_control)) {
        emu_.Get<Fatal>().Die("Sh77xxScif: SCFCR write 0x%02X at 0x%08X", value, Channel().base);
    }
    std::vector<uint8_t> out;
    bool                 drained = false;
    bool                 powered = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        RequireRunning("SCFCR", value, fcr_);
        fcr_ = value;
        if ((value & kFifoTfrst) != 0u) fifo_.ClearTransmit();
        if ((value & kFifoRfrst) != 0u) drained = fifo_.ClearReceive();
        TakeTransmitLocked(out);
        powered = powered_;
        DriveLocked();
    }
    if (drained) port_.NotifyRxDrained();
    modem_.RefreshRts();
    DeliverTx(out, powered);
}

void Sh77xxScif::WriteStatus(uint16_t value) {
    std::lock_guard<std::mutex> lk(mu_);
    if (stopped_) return;
    fifo_.WriteStatus(value, fcr_);
    DriveLocked();
}

uint16_t Sh77xxScif::ReadStatus() {
    std::lock_guard<std::mutex> lk(mu_);
    return fifo_.ReadStatus(!stopped_);
}

void Sh77xxScif::WriteTransmitData(uint8_t value) {
    std::vector<uint8_t> out;
    bool                 powered = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (stopped_) {
            emu_.Get<Fatal>().Die("Sh77xxScif: SCFTDR write 0x%02X at 0x%08X while the module clock is stopped",
                                  value, Channel().base);
        }
        if ((fcr_ & kFifoTfrst) != 0u) {
            emu_.Get<Fatal>().Die("Sh77xxScif: SCFTDR write 0x%02X at 0x%08X while TFRST holds the transmit FIFO in "
                                  "reset",
                                  value, Channel().base);
        }
        if (!fifo_.PushTransmit(value)) return;
        TakeTransmitLocked(out);
        powered = powered_;
        DriveLocked();
    }
    DeliverTx(out, powered);
}

uint8_t Sh77xxScif::ReadReceiveData() {
    uint8_t value   = 0u;
    bool    drained = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (fifo_.RxEmpty()) {
            emu_.Get<Fatal>().Die("Sh77xxScif: SCFRDR read at 0x%08X with the receive FIFO empty", Channel().base);
        }
        value = fifo_.PopReceive(fcr_, drained);
        DriveLocked();
    }
    if (drained) port_.NotifyRxDrained();
    modem_.RefreshRts();
    return value;
}

void Sh77xxScif::TakeTransmitLocked(std::vector<uint8_t>& out) {
    if ((scr_ & kCtlTe) == 0u) return;
    if (modem_.HoldsTransmit(fcr_)) return;
    fifo_.TakeTransmit(out);
    if (!IrdaModeLocked() && (smr_ & kModeChr) != 0u) {
        for (uint8_t& b : out) b = static_cast<uint8_t>(b & 0x7Fu);
    }
}

void Sh77xxScif::PostRelease() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!Sh77xxScifModemLines::Enabled(fcr_) || !fifo_.TxHeld()) return;
    }
    jobs_->PostToGuestThread([this] { ReleaseHeldTransmit(); });
}

void Sh77xxScif::ReleaseHeldTransmit() {
    std::vector<uint8_t> out;
    bool                 powered = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        TakeTransmitLocked(out);
        powered = powered_;
        DriveLocked();
    }
    DeliverTx(out, powered);
}

bool Sh77xxScif::RtsHigh() const {
    std::lock_guard<std::mutex> lk(mu_);
    return Sh77xxScifModemLines::RtsHigh(fcr_, fifo_.RxFull());
}

bool Sh77xxScif::IrdaModeLocked() const { return Channel().irda && (smr_ & kModeIrda) != 0u; }

void Sh77xxScif::DeliverTx(const std::vector<uint8_t>& bytes, bool powered) {
    if (bytes.empty()) return;
    emu_.Get<KernelDebugSink>().DeliverOrEmit(port_, bytes.data(), bytes.size(), powered, tx_line_, Channel().name);
}

void Sh77xxScif::DriveLocked() {
    intc_->SetSourceRequest(Channel().txi, (scr_ & kCtlTie) != 0u && fifo_.TransmitRequest());
    intc_->SetSourceRequest(Channel().rxi, (scr_ & kCtlRie) != 0u && fifo_.ReceiveRequest());
    intc_->SetSourceRequest(Channel().eri, false);
    intc_->SetSourceRequest(Channel().bri, false);
}

void Sh77xxScif::PushRx(const uint8_t* data, size_t n) {
    if (n == 0u) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if ((scr_ & kCtlRe) == 0u || (fcr_ & kFifoRfrst) != 0u || stopped_ || !powered_) return;
        if (IrdaModeLocked() && (scr_ & kCtlTe) != 0u) return;
        fifo_.PushReceive(data, n, fcr_);
        DriveLocked();
    }
    port_.MarkRx();
    modem_.PostRtsRefresh();
}

bool Sh77xxScif::RxEmpty() const {
    std::lock_guard<std::mutex> lk(mu_);
    return fifo_.RxEmpty();
}

void Sh77xxScif::SetRxDrainCallback(RxDrainFn cb) { port_.SetRxDrainCallback(std::move(cb)); }

void Sh77xxScif::SetModemInputs(bool cts, bool dsr, bool ri, bool dcd) {
    wiring_->SetModemInputs(Channel().base, cts, dsr, ri, dcd);
    PostRelease();
}

SerialLine::LineConfig Sh77xxScif::LineConfigLocked() const {
    LineConfig c = IrdaModeLocked() ? LineConfig{} : Sh77xxSerialFrameFormat(smr_);
    c.baud       = (scr_ & kCtlCke1) != 0u ? clock_->ClockHz(Channel()) / kExternalClocksPerBit
                                           : Sh77xxSerialBaud(smr_, brr_, cpg_->PeripheralRate(frqcr_->Value()));
    return c;
}

SerialLine::LineConfig Sh77xxScif::GetLineConfig() const {
    std::lock_guard<std::mutex> lk(mu_);
    return line_cfg_;
}

void Sh77xxScif::SetLineConfigCallback(LineConfigFn cb) { port_.SetLineConfigCallback(std::move(cb)); }

void Sh77xxScif::FireLineConfig() {
    LineConfig cfg;
    {
        std::lock_guard<std::mutex> lk(mu_);
        cfg       = LineConfigLocked();
        line_cfg_ = cfg;
    }
    port_.NotifyLineConfig(cfg);
}

void Sh77xxScif::RefreshClockStop() {
    const bool stopped = stbcr2_ != nullptr && stbcr2_->ModuleStopped(Channel().mstp);
    std::lock_guard<std::mutex> lk(mu_);
    stopped_ = stopped;
}

void Sh77xxScif::RefreshConnection() {
    const bool powered = wiring_ != nullptr && wiring_->DriverPowered(Channel().base);
    std::lock_guard<std::mutex> lk(mu_);
    powered_ = powered;
}

void Sh77xxScif::PublishEnables() {
    uint8_t control = 0u;
    {
        std::lock_guard<std::mutex> lk(mu_);
        control = scr_;
    }
    pins_->PublishEnables(Channel(), (control & kCtlTe) != 0u, (control & kCtlRe) != 0u);
}

void Sh77xxScif::SetEndpoint(SerialEndpoint* ep) { port_.SetEndpoint(ep); }

void Sh77xxScif::ArmEndpointTimer(int64_t delay_ns) { port_.ArmTimer(delay_ns); }

void Sh77xxScif::DisarmEndpointTimer() { port_.DisarmTimer(); }

bool Sh77xxScif::InitializeLocked() {
    smr_              = 0x00u;
    brr_              = 0xFFu;
    scr_              = 0x00u;
    fcr_              = 0x00u;
    const bool had_rx = fifo_.Reset();
    DriveLocked();
    return had_rx;
}

void Sh77xxScif::Initialize() {
    bool had_rx = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        had_rx = InitializeLocked();
    }
    if (had_rx) port_.NotifyRxDrained();
}

void Sh77xxScif::EnterStandby() {
    Initialize();
    PublishEnables();
    modem_.RefreshRts();
}

void Sh77xxScif::SaveState(StateWriter& w) {
    port_.SaveCradleState(w);
    {
        std::lock_guard<std::mutex> lk(mu_);
        w.Write("smr", smr_);
        w.Write("brr", brr_);
        w.Write("scr", scr_);
        w.Write("fcr", fcr_);
        fifo_.SaveState(w);
    }
    endpoint_timer_.SaveLineTimer(w, "endpoint_timer_");
    port_.SaveEndpointState(w);
}

void Sh77xxScif::RestoreState(StateReader& r) {
    emu_.Get<KernelDebugSink>().FlushLine(tx_line_, Channel().name);
    port_.RestoreCradleState(r);
    {
        std::lock_guard<std::mutex> lk(mu_);
        r.Read("smr", smr_);
        r.Read("brr", brr_);
        r.Read("scr", scr_);
        r.Read("fcr", fcr_);
        fifo_.RestoreState(r);
    }
    endpoint_timer_.RestoreLineTimer(r, "endpoint_timer_");
    port_.RestoreEndpointState(r);
}

void Sh77xxScif::PostRestore() {
    endpoint_timer_.ResumeLineTimer();
    {
        std::lock_guard<std::mutex> lk(mu_);
        DriveLocked();
    }
    Resync();
    PostRelease();
    port_.PostRestore();
}

}

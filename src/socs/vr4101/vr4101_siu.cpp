#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../peripherals/serial/serial_device_clock_timer.h"
#include "../../peripherals/serial/serial_endpoint_port.h"
#include "../../peripherals/serial/serial_line.h"
#include "../../tracing/kernel_debug_sink.h"
#include "../vr41xx/vr41xx_cmu.h"
#include "../vr41xx/vr41xx_dma_page_interrupts.h"
#include "../vr41xx/vr41xx_giu.h"
#include "../vr41xx/vr41xx_icu.h"
#include "../vr41xx/vr41xx_serial_wiring.h"
#include "vr4101_dma_control.h"
#include "vr4101_id.h"
#include "vr4101_siu_dma.h"
#include "vr4101_siu_frame.h"
#include "vr4101_siu_modem_lines.h"
#include "vr4101_siu_rx_line.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cerfcore {

namespace {

using vr4101_siu::FrameValid;
using vr4101_siu::Parity;

constexpr uint32_t kBase = 0x0B000140u;

constexpr uint32_t kOffRxDat   = 0x0u;
constexpr uint32_t kOffTxDat   = 0x2u;
constexpr uint32_t kOffCnt     = 0x4u;
constexpr uint32_t kOffDLength = 0x6u;
constexpr uint32_t kOffInt     = 0x8u;
constexpr uint32_t kOffRs232c  = 0xAu;
constexpr uint32_t kOffBaudSel = 0xCu;

constexpr uint16_t kCntTxEnable = 1u << 1;
constexpr uint16_t kCntRxEnable = 1u << 2;
constexpr uint16_t kCntIrda     = 1u << 5;
constexpr uint16_t kCntRs232    = 1u << 6;
constexpr uint16_t kCntBreak    = 1u << 13;
constexpr uint16_t kCntWritable = 0x286Eu;

constexpr uint16_t kIntTxi = 1u << 0;
constexpr uint16_t kIntTxe = 1u << 1;
constexpr uint16_t kIntRxi = 1u << 2;
constexpr uint16_t kIntRxe = 1u << 3;
constexpr uint16_t kIntRxg = 1u << 4;
constexpr uint16_t kIntRxl = 1u << 5;
constexpr Vr41xxDmaPageInterrupts kTxPages{kIntTxi, kIntTxe};
constexpr Vr41xxDmaPageInterrupts kRxPages{kIntRxi, kIntRxe};
constexpr uint16_t kIntAll = 0x07FFu;

constexpr uint16_t kRs232cRtsHigh = 1u << 0;
constexpr uint16_t kRs232cDtrHigh = 1u << 1;
constexpr uint16_t kRs232cOutputs = kRs232cRtsHigh | kRs232cDtrHigh;
constexpr uint16_t kRs232cInputs  = Vr4101SiuModemLines::kInputBits;

constexpr char kSource[] = "SIU";

class Vr4101Siu : public Peripheral, public SerialLine {
public:
    explicit Vr4101Siu(CerfEmulator& emu)
        : Peripheral(emu),
          endpoint_timer_(emu, [this] { port_.OnTimer(); }),
          port_(*this, endpoint_timer_, [this] { return ControlLines(); }) {}

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4101; }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return 0x10u; }

    void OnReady() override {
        endpoint_timer_.AttachLineTimer();
        cmu_   = &emu_.Get<Vr41xxCmu>();
        giu_   = &emu_.Get<Vr41xxGiu>();
        icu_   = &emu_.Get<Vr41xxIcu>();
        dcu_   = &emu_.Get<Vr4101DmaControl>();
        dma_   = &emu_.Get<Vr4101SiuDma>();
        rx_    = &emu_.Get<Vr4101SiuRxLine>();
        reset_ = &emu_.Get<GuestCpuReset>();
        rx_->Attach([this](uint8_t byte) { Land(byte); }, [this] { port_.NotifyRxDrained(); });
        RefreshClock();
        cmu_->RegisterClockListener(kCmuMskSiu, [this](bool supplied_after) {
            clock_on_.store(supplied_after, std::memory_order_release);
            ApplyRxTiming();
            TryTransmit();
        });
        dcu_->RegisterEnableListener(Vr4101DmaChannel::kSiuTx, [this] {
            {
                std::lock_guard<std::mutex> lk(mu_);
                tx_held_ = false;
            }
            TryTransmit();
        });
        reset_->RegisterResetListener([this](ResetLineKind) { ResetUnit(); });
        reset_->RegisterResetReleaseListener([this] {
            RefreshClock();
            FireLineConfig();
            port_.NotifyControlLines();
            port_.ResendModemInputs();
        });
        emu_.Get<PeripheralDispatcher>().Register(this);

        wiring_ = emu_.TryGet<Vr41xxSerialWiring>();
        if (wiring_ != nullptr) modem_ = wiring_->ForSiu();
        if (!modem_) return;
        wiring_->RegisterSupplyListener([this] {
            ApplyModemInputs();
            ApplyRxTiming();
            port_.NotifyControlLines();
        });
        SetModemInputs(false, false, false, false);
        port_.AttachCradle(emu_, modem_->port);
        LOG(Periph, "[UART] VR4101 SIU attached as serial endpoint line '%s'\n", modem_->port.name.c_str());
    }

    void OnShutdown() override {
        port_.OnShutdown();
        emu_.Get<KernelDebugSink>().FlushLine(tx_line_, kSource);
    }

    uint16_t ReadHalf(uint32_t addr) override {
        rx_->Settle();
        std::lock_guard<std::mutex> lk(mu_);
        switch (addr - kBase) {
            case kOffRxDat:   return rxdat_;
            case kOffTxDat:   return txdat_;
            case kOffCnt:     return cnt_;
            case kOffDLength: return dlength_;
            case kOffInt:     return int_;
            case kOffRs232c:  return Rs232cLocked();
            case kOffBaudSel: return baudsel_;
            default:          HaltUnsupportedAccess("VR4101 SIU read", addr, 0);
        }
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        switch (addr - kBase) {
            case kOffCnt:     WriteCnt(addr, value); return;
            case kOffDLength: WriteFrame(addr, value, vr4101_siu::kDLenWritable, dlength_); return;
            case kOffInt:     WriteInt(addr, value); return;
            case kOffRs232c:  WriteRs232c(addr, value); return;
            case kOffBaudSel: WriteFrame(addr, value, vr4101_siu::kBaudSelField, baudsel_); return;
            default:          HaltUnsupportedAccess("VR4101 SIU write", addr, value);
        }
    }

    void PushRx(const uint8_t* data, size_t n) override {
        if (n == 0u) return;
        bool pushed = false;
        {
            std::lock_guard<std::mutex> lk(mu_);
            pushed = ReceivingLocked() && rx_->Push(data, n);
        }
        if (pushed) port_.MarkRx();
    }

    bool RxEmpty() const override { return rx_->Idle(); }
    void SetRxDrainCallback(RxDrainFn cb) override { port_.SetRxDrainCallback(std::move(cb)); }

    void SetModemInputs(bool cts, bool dsr, bool, bool dcd) override {
        if (!modem_) return;
        {
            std::lock_guard<std::mutex> lk(mu_);
            modem_lines_.SetConnector(cts, dsr);
        }
        giu_->SetPinLevel(modem_->carrier_gpio, dcd);
        ApplyModemInputs();
    }

    LineConfig GetLineConfig() const override {
        std::lock_guard<std::mutex> lk(mu_);
        return vr4101_siu::LineConfigOf(dlength_, baudsel_, nine_bit_parity_);
    }

    void SetLineConfigCallback(LineConfigFn cb) override { port_.SetLineConfigCallback(std::move(cb)); }
    void SetEndpoint(SerialEndpoint* ep) override { port_.SetEndpoint(ep); }
    void ArmEndpointTimer(int64_t delay_ns) override { port_.ArmTimer(delay_ns); }
    void DisarmEndpointTimer() override { port_.DisarmTimer(); }

    void SaveState(StateWriter& w) override {
        port_.SaveCradleState(w);
        {
            std::lock_guard<std::mutex> lk(mu_);
            w.Write("siu_cnt", cnt_);
            w.Write("siu_dlength", dlength_);
            w.Write("siu_int", int_);
            w.Write("siu_rs232c_out", rs232c_out_);
            w.Write("siu_baudsel", baudsel_);
            w.Write("siu_rxdat", rxdat_);
            w.Write("siu_txdat", txdat_);
            w.Write<uint8_t>("siu_tx_held", tx_held_ ? 1u : 0u);
            w.Write<uint8_t>("siu_nine_bit_odd", nine_bit_parity_ == Parity::Odd ? 1u : 0u);
            modem_lines_.Save(w);
        }
        rx_->Save(w);
        endpoint_timer_.SaveLineTimer(w, "endpoint_timer_");
        port_.SaveEndpointState(w);
    }

    void RestoreState(StateReader& r) override {
        emu_.Get<KernelDebugSink>().FlushLine(tx_line_, kSource);
        port_.RestoreCradleState(r);
        {
            std::lock_guard<std::mutex> lk(mu_);
            r.Read("siu_cnt", cnt_);
            r.Read("siu_dlength", dlength_);
            r.Read("siu_int", int_);
            r.Read("siu_rs232c_out", rs232c_out_);
            r.Read("siu_baudsel", baudsel_);
            r.Read("siu_rxdat", rxdat_);
            r.Read("siu_txdat", txdat_);
            uint8_t held = 0u, odd = 0u;
            r.Read("siu_tx_held", held);
            r.Read("siu_nine_bit_odd", odd);
            tx_held_         = held != 0u;
            nine_bit_parity_ = odd != 0u ? Parity::Odd : Parity::Even;
            modem_lines_.Restore(r);
        }
        rx_->Restore(r);
        endpoint_timer_.RestoreLineTimer(r, "endpoint_timer_");
        port_.RestoreEndpointState(r);
    }

    void PostRestore() override {
        endpoint_timer_.ResumeLineTimer();
        RefreshClock();
        PublishInterrupts();
        port_.PostRestore();
    }

private:
    void RefreshClock() { clock_on_.store(cmu_->SuppliedClocks(kCmuMskSiu) != 0u, std::memory_order_release); }

    bool ClockSupplied() const { return clock_on_.load(std::memory_order_acquire); }
    bool SupplyOn() const { return wiring_ != nullptr && modem_ && wiring_->SupplyOn(); }

    bool Rs232PathLocked() const { return (cnt_ & kCntRs232) != 0u && (cnt_ & kCntIrda) == 0u; }

    bool ReceivingLocked() const {
        return (cnt_ & kCntRxEnable) != 0u && Rs232PathLocked() && SupplyOn() && ClockSupplied() &&
               FrameValid(dlength_);
    }

    uint16_t Rs232cLocked() const { return static_cast<uint16_t>(rs232c_out_ | modem_lines_.Rs232cInputs()); }

    SerialControlLines ControlLines() {
        if (!SupplyOn()) return SerialControlLines{};
        std::lock_guard<std::mutex> lk(mu_);
        SerialControlLines lines;
        lines.dtr      = (rs232c_out_ & kRs232cDtrHigh) == 0u;
        lines.rts      = (rs232c_out_ & kRs232cRtsHigh) == 0u;
        lines.in_break = (cnt_ & kCntBreak) != 0u && Rs232PathLocked();
        return lines;
    }

    void WriteCnt(uint32_t addr, uint16_t value) {
        if ((value & ~kCntWritable) != 0u) HaltUnsupportedAccess("VR4101 SIU SIUCNTREG reserved bit", addr, value);
        if ((value & kCntIrda) != 0u && (value & kCntRs232) != 0u) {
            HaltUnsupportedAccess("VR4101 SIU SIUCNTREG selects both IrDA and RS-232-C", addr, value);
        }
        rx_->Settle();
        bool lines_moved = false;
        {
            std::lock_guard<std::mutex> lk(mu_);
            lines_moved = ((value ^ cnt_) & (kCntBreak | kCntRs232 | kCntIrda)) != 0u;
            cnt_        = value;
        }
        ApplyRxTiming();
        if (lines_moved) port_.NotifyControlLines();
        TryTransmit();
    }

    void WriteFrame(uint32_t addr, uint16_t value, uint16_t writable, uint16_t& reg) {
        if ((value & ~writable) != 0u) HaltUnsupportedAccess("VR4101 SIU frame register reserved bit", addr, value);
        rx_->Settle();
        {
            std::lock_guard<std::mutex> lk(mu_);
            reg = value;
        }
        FireLineConfig();
    }

    void WriteInt(uint32_t addr, uint16_t value) {
        if ((value & ~kIntAll) != 0u) HaltUnsupportedAccess("VR4101 SIU SIUINTREG reserved bit", addr, value);
        rx_->Settle();
        {
            std::lock_guard<std::mutex> lk(mu_);
            int_ = static_cast<uint16_t>(int_ & ~value);
        }
        PublishInterrupts();
    }

    void WriteRs232c(uint32_t addr, uint16_t value) {
        if ((value & ~(kRs232cOutputs | kRs232cInputs)) != 0u) {
            HaltUnsupportedAccess("VR4101 SIU SIURS232CREG reserved bit", addr, value);
        }
        {
            std::lock_guard<std::mutex> lk(mu_);
            rs232c_out_ = static_cast<uint16_t>(value & kRs232cOutputs);
        }
        port_.NotifyControlLines();
    }

    void TryTransmit() {
        uint16_t dlength = 0u;
        bool     wire    = false;
        {
            std::lock_guard<std::mutex> lk(mu_);
            const bool wants   = (cnt_ & kCntTxEnable) != 0u && !tx_held_ && ClockSupplied();
            const bool granted = wants && dcu_->TransferEnabled(Vr4101DmaChannel::kSiuTx);
            dcu_->SetRequest(Vr4101DmaChannel::kSiuTx, wants && !granted);
            if (!granted) return;
            dlength = dlength_;
            wire    = Rs232PathLocked() && (cnt_ & kCntBreak) == 0u;
        }
        if (!FrameValid(dlength)) emu_.Get<Fatal>().Die("VR4101 SIU transmit with SIUDLENGTHREG 0x%04X", dlength);
        wire = wire && SupplyOn();
        const Vr4101SiuDma::TxRun run = dma_->Transmit(dlength);
        {
            std::lock_guard<std::mutex> lk(mu_);
            txdat_   = run.last_word;
            tx_held_ = true;
            if (run.boundary) int_ |= kTxPages.AtBoundary(int_);
        }
        PublishInterrupts();
        Deliver(run.chars, dlength, wire);
    }

    void Deliver(const std::vector<vr4101_siu::TxChar>& chars, uint16_t dlength, bool wire) {
        const bool           nine = vr4101_siu::NineBitPayload(dlength);
        std::vector<uint8_t> out;
        for (const vr4101_siu::TxChar& c : chars) {
            if (nine) {
                bool moved = false;
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    const Parity carrying = vr4101_siu::ParityCarrying(c, nine_bit_parity_);
                    moved                 = carrying != nine_bit_parity_;
                    nine_bit_parity_      = carrying;
                }
                if (moved) {
                    Emit(out, wire);
                    port_.NotifyLineConfig(GetLineConfig());
                }
            }
            out.push_back(c.data);
        }
        Emit(out, wire);
    }

    void Emit(std::vector<uint8_t>& out, bool wire) {
        if (out.empty()) return;
        emu_.Get<KernelDebugSink>().DeliverOrEmit(port_, out.data(), out.size(), wire, tx_line_, kSource);
        out.clear();
    }

    void Land(uint8_t byte) {
        uint16_t raised = 0u;
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!ReceivingLocked()) return;
            rxdat_ = vr4101_siu::ReceivedWord(dlength_, baudsel_, nine_bit_parity_, byte);
            if (!dcu_->TransferEnabled(Vr4101DmaChannel::kSiuRx)) {
                raised = kIntRxl;
            } else {
                raised = kIntRxg;
                if (dma_->Receive(rxdat_)) raised |= kRxPages.AtBoundary(int_);
            }
            int_ |= raised;
        }
        PublishInterrupts();
    }

    void ApplyModemInputs() {
        const bool powered = SupplyOn();
        const bool dcd     = modem_ && giu_->GetPinLevel(modem_->carrier_gpio);
        const bool latch   = !reset_->Delivering();
        uint16_t   raised  = 0u;
        {
            std::lock_guard<std::mutex> lk(mu_);
            raised = modem_lines_.Apply(powered, dcd);
            if (!latch) raised = 0u;
            int_ |= raised;
        }
        if (raised != 0u) PublishInterrupts();
    }

    void ApplyRxTiming() {
        Vr4101SiuRxLine::Timing timing;
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (ReceivingLocked()) {
                timing.baud             = vr4101_siu::Baud(baudsel_);
                timing.bits_before_stop = vr4101_siu::RxBits(dlength_);
                timing.frame_bits       = vr4101_siu::TxBits(dlength_);
            }
        }
        rx_->SetTiming(timing);
    }

    void FireLineConfig() {
        ApplyRxTiming();
        port_.NotifyLineConfig(GetLineConfig());
    }

    void PublishInterrupts() {
        uint16_t bits = 0u;
        {
            std::lock_guard<std::mutex> lk(mu_);
            bits = int_;
        }
        icu_->SetDsiuSource(bits);
    }

    void ResetUnit() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            cnt_ = dlength_ = int_ = rs232c_out_ = baudsel_ = rxdat_ = txdat_ = 0u;
            tx_held_         = false;
            nine_bit_parity_ = Parity::Even;
        }
        rx_->Reset();
        ApplyModemInputs();
        PublishInterrupts();
    }

    mutable std::mutex mu_;
    uint16_t           cnt_             = 0u;
    uint16_t           dlength_         = 0u;
    uint16_t           int_             = 0u;
    uint16_t           rs232c_out_      = 0u;
    uint16_t           baudsel_         = 0u;
    uint16_t           rxdat_           = 0u;
    uint16_t           txdat_           = 0u;
    bool                tx_held_         = false;
    Parity              nine_bit_parity_ = Parity::Even;
    Vr4101SiuModemLines modem_lines_;
    std::atomic<bool>   clock_on_{false};
    std::string        tx_line_;

    SerialDeviceClockTimer endpoint_timer_;
    SerialEndpointPort     port_;

    std::optional<Vr41xxSerialModem> modem_;
    Vr41xxSerialWiring*              wiring_ = nullptr;
    Vr41xxCmu*                       cmu_    = nullptr;
    Vr41xxGiu*                       giu_    = nullptr;
    Vr41xxIcu*                       icu_    = nullptr;
    Vr4101DmaControl*                dcu_    = nullptr;
    Vr4101SiuDma*                    dma_    = nullptr;
    Vr4101SiuRxLine*                 rx_     = nullptr;
    GuestCpuReset*                   reset_  = nullptr;
};

}

REGISTER_SERVICE(Vr4101Siu);

}

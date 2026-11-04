#include "ibm_trackpoint.h"

#include "../serial/uart_frame.h"
#include <cerfcore/state/state_stream.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace cerfcore {

namespace {

using Parity = SerialLine::LineConfig::Parity;

constexpr uint8_t kController      = 0xE2u;
constexpr uint8_t kReadExtendedId  = 0xD0u;
constexpr uint8_t kReadSecondaryId = 0xE1u;
constexpr uint8_t kSerialBit7      = 0x80u;

constexpr uint8_t kE2LastRamRead = 0x3Fu;
constexpr uint8_t kE2BitToggle   = 0x47u;
constexpr uint8_t kE2Ps2Mode     = 0x53u;
constexpr uint8_t kE2RamRead     = 0x80u;
constexpr uint8_t kE2RamWrite    = 0x81u;

constexpr uint32_t kSerialBaud     = 1200u;
constexpr uint32_t kPs2BaudClock   = 14745600u;
constexpr uint32_t kPs2BaudDivisor = 384u;

constexpr uint8_t kRamConfig      = 0x2Cu;
constexpr uint8_t kRamXmitGap     = 0x62u;
constexpr uint8_t kXmitGapDefault = 0x09u;
constexpr uint8_t kPressToSelect  = 0x01u;

constexpr Ps2MouseTraits kTrackpointTraits = {true, false};

constexpr std::array<uint8_t, 76> kSerialMouseId = {
    0x4D, 0x33, 0x00, 0x11, 0x19, 0x19, 0x18, 0x10, 0x15, 0x10, 0x11, 0x00, 0x32, 0x33, 0x2F,
    0x08, 0x01, 0x24, 0x29, 0x22, 0x2D, 0x13, 0x17, 0x18, 0x13, 0x3C, 0x3C, 0x2D, 0x2F, 0x35,
    0x33, 0x25, 0x3C, 0x30, 0x2E, 0x30, 0x10, 0x26, 0x11, 0x17, 0x3C, 0x29, 0x22, 0x2D, 0x00,
    0x34, 0x32, 0x21, 0x23, 0x2B, 0x30, 0x2F, 0x29, 0x2E, 0x34, 0x00, 0x36, 0x25, 0x32, 0x33,
    0x29, 0x2F, 0x2E, 0x00, 0x14, 0x0E, 0x10, 0x00, 0x39, 0x2B, 0x34, 0x13, 0x23, 0x14, 0x15,
    0x09};

constexpr std::array<uint8_t, 13> kUnmodeledE2 = {0x40, 0x41, 0x44, 0x45, 0x46, 0x4B, 0x4C,
                                                  0x4E, 0x4F, 0x51, 0x52, 0x7F, 0xB9};
constexpr std::array<uint8_t, 11> kWritableRam = {0x41, 0x42, 0x4A, 0x58, 0x59, 0x5A,
                                                  0x5C, 0x5D, 0x5E, 0x60, 0x62};

template <size_t N>
bool Contains(const std::array<uint8_t, N>& set, uint8_t b) {
    return std::find(set.begin(), set.end(), b) != set.end();
}

}

IbmTrackpoint::IbmTrackpoint(HostFormatFn host_format, SendFn send, HaltFn halt)
    : host_format_(std::move(host_format)),
      send_(std::move(send)),
      halt_(std::move(halt)),
      ps2_(nullptr, [this](const char* what, unsigned value) { halt_(what, value); },
           kTrackpointTraits) {}

void IbmTrackpoint::PowerOn() {
    mode_       = Mode::kSerial;
    baud_param_ = 0;
    ram_.fill(0);
    ram_known_.fill(0);
    ram_[kRamConfig]        = 0;
    ram_known_[kRamConfig]  = 1;
    ram_[kRamXmitGap]       = kXmitGapDefault;
    ram_known_[kRamXmitGap] = 1;
    ps2_.Reset();
    parse_          = Parse::kCommand;
    e2_command_     = 0;
    e2_arg1_        = 0;
    id_in_progress_ = false;
    pending_ps2_    = false;
    pending_param_  = 0;
    tx_queue_.clear();
}

void IbmTrackpoint::SetLines(bool dtr_high, bool rts_high) {
    const bool rts_fell = rts_high_ && !rts_high;
    dtr_high_ = dtr_high;
    rts_high_ = rts_high;
    if (rts_fell) SerialMouseReset();
    Flush();
}

void IbmTrackpoint::OnGuestTx(uint8_t byte) {
    const SerialLine::LineConfig host = host_format_();
    const SerialLine::LineConfig own  = Format();
    if (host.baud != own.baud) halt_("host baud rate", host.baud);
    if (id_in_progress_) return;
    if (pending_ps2_) halt_("byte before the PS/2 mode ACK was sent", byte);
    tx_queue_.clear();
    const UartDecodedFrame frame = DecodeUartFrame(host, byte, own);
    if (frame.parity_error || frame.framing_error) {
        Reject();
    } else {
        OnByte(frame.data);
    }
    while (ps2_.HasData()) tx_queue_.push_back(ps2_.ReadData());
    Flush();
}

SerialLine::LineConfig IbmTrackpoint::Format() const {
    SerialLine::LineConfig f;
    f.data_bits = 8;
    f.stop      = SerialLine::LineConfig::Stop::One;
    if (mode_ == Mode::kSerial) {
        f.baud   = kSerialBaud;
        f.parity = Parity::None;
    } else {
        f.baud   = kPs2BaudClock / (kPs2BaudDivisor * (256u - baud_param_));
        f.parity = Parity::Odd;
    }
    return f;
}

void IbmTrackpoint::SerialMouseReset() {
    mode_           = Mode::kSerial;
    pending_ps2_    = false;
    parse_          = Parse::kCommand;
    id_in_progress_ = true;
    ps2_.AbortCommand();
    while (ps2_.HasData()) ps2_.ReadData();
    tx_queue_.assign(kSerialMouseId.begin(), kSerialMouseId.end());
}

void IbmTrackpoint::OnByte(uint8_t b) {
    switch (parse_) {
        case Parse::kCommand: OnCommand(b); break;
        case Parse::kE2Sub:   OnE2Sub(b);   break;
        case Parse::kE2Arg1:  OnE2Arg1(b);  break;
        case Parse::kE2Arg2:  OnE2Arg2(b);  break;
    }
}

void IbmTrackpoint::OnCommand(uint8_t b) {
    if (ps2_.AwaitsCommand()) {
        if (b == kController) {
            ps2_.Acknowledge();
            parse_ = Parse::kE2Sub;
            return;
        }
        if (b == kReadExtendedId || b == kReadSecondaryId) halt_("command", b);
    }
    ps2_.WriteCommand(b);
}

void IbmTrackpoint::OnE2Sub(uint8_t b) {
    if (b <= kE2LastRamRead) {
        ps2_.Acknowledge({ReadRam(b)});
        parse_ = Parse::kCommand;
        return;
    }
    if (b == kE2BitToggle || b == kE2Ps2Mode || b == kE2RamRead || b == kE2RamWrite) {
        e2_command_ = b;
        ps2_.Acknowledge();
        parse_ = Parse::kE2Arg1;
        return;
    }
    if (Contains(kUnmodeledE2, b)) halt_("E2 command", b);
    Reject();
}

void IbmTrackpoint::OnE2Arg1(uint8_t b) {
    if (e2_command_ == kE2Ps2Mode) {
        ps2_.Acknowledge();
        pending_ps2_   = true;
        pending_param_ = b;
        parse_         = Parse::kCommand;
        return;
    }
    if (e2_command_ == kE2RamRead) {
        ps2_.Acknowledge({ReadRam(b)});
        parse_ = Parse::kCommand;
        return;
    }
    e2_arg1_ = b;
    ps2_.Acknowledge();
    parse_ = Parse::kE2Arg2;
}

void IbmTrackpoint::OnE2Arg2(uint8_t b) {
    const unsigned operands = (static_cast<unsigned>(e2_arg1_) << 8) | b;
    if (e2_command_ == kE2BitToggle) {
        if (e2_arg1_ != kRamConfig || b != kPressToSelect) halt_("E2 47 bit toggle", operands);
        ram_[kRamConfig] = static_cast<uint8_t>(ram_[kRamConfig] ^ b);
    } else {
        if (!Contains(kWritableRam, e2_arg1_)) halt_("E2 81 RAM write", operands);
        ram_[e2_arg1_]       = b;
        ram_known_[e2_arg1_] = 1;
    }
    ps2_.Acknowledge();
    parse_ = Parse::kCommand;
}

void IbmTrackpoint::Reject() {
    if (ps2_.Reject()) parse_ = Parse::kCommand;
}

void IbmTrackpoint::Flush() {
    if (dtr_high_) return;
    while (!tx_queue_.empty()) {
        uint8_t b = tx_queue_.front();
        tx_queue_.pop_front();
        if (mode_ == Mode::kSerial) b = static_cast<uint8_t>(b | kSerialBit7);
        send_(b, Format());
    }
    id_in_progress_ = false;
    if (pending_ps2_) {
        mode_        = Mode::kPs2;
        baud_param_  = pending_param_;
        pending_ps2_ = false;
    }
}

uint8_t IbmTrackpoint::ReadRam(uint8_t addr) {
    if (ram_known_[addr] == 0u) halt_("RAM read", addr);
    return ram_[addr];
}

void IbmTrackpoint::SaveState(StateWriter& w) {
    const std::vector<uint8_t> queued(tx_queue_.begin(), tx_queue_.end());
    w.Write("tp_mode", mode_);
    w.Write("tp_baud_param", baud_param_);
    w.Write("tp_ram", ram_);
    w.Write("tp_ram_known", ram_known_);
    w.Write("tp_parse", parse_);
    w.Write("tp_e2_command", e2_command_);
    w.Write("tp_e2_arg1", e2_arg1_);
    w.Write<uint32_t>("tp_tx_queue_size", static_cast<uint32_t>(queued.size()));
    w.WriteBytes("tp_tx_queue", queued.data(), queued.size());
    w.Write<uint8_t>("tp_id_in_progress", id_in_progress_ ? 1u : 0u);
    w.Write<uint8_t>("tp_pending_ps2", pending_ps2_ ? 1u : 0u);
    w.Write("tp_pending_param", pending_param_);
    w.Write<uint8_t>("tp_dtr_high", dtr_high_ ? 1u : 0u);
    w.Write<uint8_t>("tp_rts_high", rts_high_ ? 1u : 0u);
    ps2_.SaveState(w);
}

void IbmTrackpoint::RestoreState(StateReader& r) {
    uint32_t tx_queue_size  = 0;
    uint8_t  id_in_progress = 0;
    uint8_t  pending_ps2    = 0;
    uint8_t  dtr_high       = 0;
    uint8_t  rts_high       = 0;
    r.Read("tp_mode", mode_);
    r.Read("tp_baud_param", baud_param_);
    r.Read("tp_ram", ram_);
    r.Read("tp_ram_known", ram_known_);
    r.Read("tp_parse", parse_);
    r.Read("tp_e2_command", e2_command_);
    r.Read("tp_e2_arg1", e2_arg1_);
    r.Read("tp_tx_queue_size", tx_queue_size);
    std::vector<uint8_t> queued(tx_queue_size);
    r.ReadBytes("tp_tx_queue", queued.data(), queued.size());
    tx_queue_.assign(queued.begin(), queued.end());
    r.Read("tp_id_in_progress", id_in_progress);
    r.Read("tp_pending_ps2", pending_ps2);
    r.Read("tp_pending_param", pending_param_);
    r.Read("tp_dtr_high", dtr_high);
    r.Read("tp_rts_high", rts_high);
    id_in_progress_ = id_in_progress != 0u;
    pending_ps2_    = pending_ps2 != 0u;
    dtr_high_       = dtr_high != 0u;
    rts_high_       = rts_high != 0u;
    ps2_.RestoreState(r);
}

}

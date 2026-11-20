#include "modem_personality.h"

#include "at_decimal.h"
#include "serial_line.h"
#include "ppp_terminator.h"
#include "../external_device_clock.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/state/state_stream.h>

#include <algorithm>
#include <cctype>
#include <vector>

namespace cerfcore {

namespace {

constexpr int64_t  kNsPer10Ms    = 10'000'000;
constexpr int64_t  kNsPer20Ms    = 20'000'000;
constexpr int64_t  kNoDue        = INT64_MAX;
constexpr size_t   kBodyLimit    = 256u;
constexpr uint8_t  kEscapeLength = 3u;

constexpr size_t kRegEscape     = ModemSRegisters::kEscape;
constexpr size_t kRegTerminator = ModemSRegisters::kTerminator;
constexpr size_t kRegFormat     = ModemSRegisters::kFormat;
constexpr size_t kRegEdit       = ModemSRegisters::kEdit;
constexpr size_t kRegGuard      = ModemSRegisters::kGuard;
constexpr size_t kRegDtrDetect  = ModemSRegisters::kDtrDetect;

constexpr char kIfcName[] = "IFC";

void WriteText(StateWriter& w, const char* count_key, const char* bytes_key, const std::string& s) {
    w.Write<uint32_t>(count_key, static_cast<uint32_t>(s.size()));
    if (!s.empty()) w.WriteBytes(bytes_key, s.data(), s.size());
}

void ReadText(StateReader& r, const char* count_key, const char* bytes_key, std::string& s) {
    uint32_t n = 0;
    r.Read(count_key, n);
    s.assign(n, '\0');
    if (n) r.ReadBytes(bytes_key, s.data(), n);
}

}  /* namespace */

ModemPersonality::ModemPersonality(CerfEmulator& emu, std::string net_id)
    : emu_(emu), net_id_(std::move(net_id)) {
    LoadDefaults();
}

ModemPersonality::~ModemPersonality() = default;

void ModemPersonality::OnOpen() {
    line_.clear();
    last_body_.clear();
    prefix_           = Prefix::None;
    line_overflow_    = false;
    online_           = false;
    data_mode_        = false;
    dtr_loss_pending_ = false;
    escape_count_     = 0u;
    LoadDefaults();
    PushModemInputs();
}

void ModemPersonality::ResendModemInputs() { PushModemInputs(); }

void ModemPersonality::OnClose() {
    uart_->SetModemInputs(false, false, false, false);
    online_           = false;
    data_mode_        = false;
    dtr_loss_pending_ = false;
    escape_count_     = 0u;
    if (terminator_) terminator_->Stop();
}

void ModemPersonality::OnControlLines(bool dtr, bool rts) {
    const bool fell = dtr_ && !dtr;
    const bool rose = !dtr_ && dtr;
    dtr_ = dtr;
    rts_ = rts;
    UpdateReceiveReady();
    if (rose && dtr_loss_pending_) {
        dtr_loss_pending_ = false;
        ArmTimer();
    }
    if (!fell || !data_mode_ || dtr_mode_ == 0u) return;
    if (s_[kRegDtrDetect] == 0u) {
        OnDtrLoss();
        return;
    }
    dtr_loss_pending_ = true;
    dtr_loss_due_     = NowNs() + int64_t{s_[kRegDtrDetect]} * kNsPer10Ms;
    ArmTimer();
}

void ModemPersonality::OnEndpointTimer() {
    const int64_t now = NowNs();
    if (dtr_loss_pending_ && now >= dtr_loss_due_) {
        dtr_loss_pending_ = false;
        OnDtrLoss();
    }
    if (escape_count_ == kEscapeLength && now >= escape_due_) EnterOnlineCommandState();
    ArmTimer();
}

void ModemPersonality::OnDtrLoss() {
    if (dtr_mode_ == 1u) EnterOnlineCommandState();
    else                 SetCarrier(false);
}

void ModemPersonality::OnBreak(bool on) {
    if (!on || !data_mode_) return;
    terminator_->OnGuestBreak();
}

void ModemPersonality::OnGuestTx(const uint8_t* data, size_t n) {
    if (!data_mode_) {
        for (size_t i = 0; i < n; ++i) HandleCommandByte(data[i]);
        return;
    }
    std::vector<uint8_t> passed;
    const bool           ready_moved = flow_.TakeGuestData(data, n, passed);
    const int64_t        now         = NowNs();
    bool                 rearm       = false;
    for (size_t i = 0; i < n; ++i) rearm = OnGuestDataByte(data[i], now) || rearm;
    if (ready_moved) UpdateReceiveReady();
    if (rearm) ArmTimer();
    if (!passed.empty()) terminator_->OnGuestData(passed.data(), passed.size());
}

bool ModemPersonality::OnGuestDataByte(uint8_t b, int64_t now) {
    const uint8_t escape = s_[kRegEscape];
    const int64_t guard  = int64_t{s_[kRegGuard]} * kNsPer20Ms;
    const bool    was    = escape_count_ == kEscapeLength;
    if (escape > 127u || b != escape || escape_count_ == kEscapeLength) {
        escape_count_ = 0u;
    } else if (escape_count_ > 0u || now - last_tx_ns_ >= guard) {
        ++escape_count_;
        if (escape_count_ == kEscapeLength) escape_due_ = now + guard;
    }
    last_tx_ns_ = now;
    return was != (escape_count_ == kEscapeLength);
}

void ModemPersonality::ArmTimer() {
    int64_t due = kNoDue;
    if (dtr_loss_pending_) due = dtr_loss_due_;
    if (escape_count_ == kEscapeLength) due = std::min(due, escape_due_);
    if (due == kNoDue) {
        uart_->DisarmEndpointTimer();
        return;
    }
    const int64_t now = NowNs();
    uart_->ArmEndpointTimer(due > now ? due - now : 0);
}

int64_t ModemPersonality::NowNs() { return emu_.Get<ExternalDeviceClock>().NowNs(); }

void ModemPersonality::HandleCommandByte(uint8_t b) {
    if (echo_) uart_->PushRx(&b, 1);
    if (b == s_[kRegTerminator]) {
        if (prefix_ == Prefix::Body) ProcessCommandLine();
        prefix_ = Prefix::None;
        return;
    }
    if (b == s_[kRegEdit]) {
        if (prefix_ != Prefix::Body)  prefix_ = Prefix::None;
        else if (!line_.empty())      line_.pop_back();
        return;
    }
    if (prefix_ == Prefix::Body) {
        if (line_.size() < kBodyLimit) line_.push_back(static_cast<char>(b));
        else                           line_overflow_ = true;
        return;
    }
    if ((prefix_ == Prefix::UpperA && b == 'T') || (prefix_ == Prefix::LowerA && b == 't')) {
        prefix_ = Prefix::Body;
        line_.clear();
        line_overflow_ = false;
        return;
    }
    if (prefix_ != Prefix::None && b == '/') {
        prefix_ = Prefix::None;
        ExecuteBody(last_body_);
        return;
    }
    prefix_ = b == 'A' ? Prefix::UpperA : b == 'a' ? Prefix::LowerA : Prefix::None;
}

void ModemPersonality::ProcessCommandLine() {
    std::string body;
    body.reserve(line_.size());
    for (char c : line_) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc > 0x20u) body.push_back(static_cast<char>(toupper(uc)));
    }
    const bool overflow = line_overflow_;
    line_.clear();
    line_overflow_ = false;

    LOG(Periph, "[Modem] cmd \"AT%s\"\n", body.c_str());

    if (overflow) {
        Reply(Result::Error);
        return;
    }
    last_body_ = body;
    ExecuteBody(last_body_);
}

void ModemPersonality::ExecuteBody(const std::string& body) {
    size_t i = 0;
    while (i < body.size()) {
        const char c = body[i++];
        unsigned   v = 0;
        switch (c) {
            case 'E':
                if (!ReadAtValue(body, i, 1u, v)) return Reply(Result::Error);
                echo_ = v != 0u;
                break;
            case 'Q':
                if (!ReadAtValue(body, i, 1u, v)) return Reply(Result::Error);
                quiet_ = v != 0u;
                break;
            case 'V':
                if (!ReadAtValue(body, i, 1u, v)) return Reply(Result::Error);
                verbose_ = v != 0u;
                break;
            case 'X':
                if (!ReadAtValue(body, i, 4u, v)) return Reply(Result::Error);
                break;
            case 'L':
                if (!ReadAtValue(body, i, 3u, v)) return Reply(Result::Error);
                break;
            case 'M':
                if (!ReadAtValue(body, i, 2u, v)) return Reply(Result::Error);
                break;
            case 'T':
            case 'P':
                if (i < body.size() && isdigit(static_cast<unsigned char>(body[i])))
                    return Reply(Result::Error);
                break;
            case 'H':
                if (!ReadAtValue(body, i, 0u, v)) return Reply(Result::Error);
                SetCarrier(false);
                break;
            case 'O':
                if (!ReadAtValue(body, i, 0u, v)) return Reply(Result::Error);
                if (!online_) return Reply(Result::NoCarrier);
                Reply(Result::Connect);
                EnterDataMode();
                return;
            case 'A':
                return Reply(online_ ? Result::Error : Result::NoCarrier);
            case 'D': {
                if (online_) return Reply(Result::Error);
                const size_t semi = body.find(';', i);
                if (semi == std::string::npos) {
                    Reply(Result::Connect);
                    SetCarrier(true);
                    return;
                }
                i = semi + 1u;
                break;
            }
            case 'Z':
                if (!ReadAtValue(body, i, 0u, v)) return Reply(Result::Error);
                if (online_) SetCarrier(false);
                LoadDefaults();
                PushModemInputs();
                return Reply(Result::Ok);
            case 'S':
            case '+': {
                std::string info;
                const bool  ok = c == 'S' ? s_.Execute(body, i, info) : ExecuteExtended(body, i, info);
                if (!ok) return Reply(Result::Error);
                if (!info.empty()) PushInfo(info.c_str());
                break;
            }
            case '&': {
                if (i >= body.size()) return Reply(Result::Error);
                const char a = body[i++];
                if (a == 'C') {
                    if (!ReadAtValue(body, i, 1u, v)) return Reply(Result::Error);
                    dcd_mode_ = static_cast<uint8_t>(v);
                    PushModemInputs();
                } else if (a == 'D') {
                    if (!ReadAtValue(body, i, 2u, v)) return Reply(Result::Error);
                    dtr_mode_ = static_cast<uint8_t>(v);
                } else if (a == 'F') {
                    if (!ReadAtValue(body, i, 0u, v)) return Reply(Result::Error);
                    LoadDefaults();
                    PushModemInputs();
                } else if (a == 'K') {
                    i += ParseAtDecimal(body, i, v);
                    if (!flow_.SelectAmpK(v)) return Reply(Result::Error);
                } else {
                    return Reply(Result::Error);
                }
                break;
            }
            default:
                return Reply(Result::Error);
        }
    }
    Reply(Result::Ok);
}

bool ModemPersonality::ExecuteExtended(const std::string& body, size_t& i, std::string& info) {
    const size_t start = i;
    while (i < body.size() && body[i] != '=' && body[i] != '?' && body[i] != ';') ++i;
    if (body.compare(start, i - start, kIfcName) != 0 || !flow_.ExecuteIfc(body, i, info)) return false;
    if (i == body.size()) return true;
    if (body[i] != ';') return false;
    ++i;
    return true;
}

void ModemPersonality::LoadDefaults() {
    s_.LoadDefaults();
    echo_     = true;
    quiet_    = false;
    verbose_  = true;
    dcd_mode_ = 1u;
    dtr_mode_ = 2u;
    flow_.LoadDefaults();
}

void ModemPersonality::PushInfo(const char* text) {
    const char cr = static_cast<char>(s_[kRegTerminator]);
    const char lf = static_cast<char>(s_[kRegFormat]);
    std::string out;
    if (verbose_) out.append(1, cr).append(1, lf);
    out.append(text).append(1, cr).append(1, lf);
    PushStr(out);
}

void ModemPersonality::Reply(Result result) {
    const char* text = "OK";
    char        code = '0';
    switch (result) {
        case Result::Ok:        text = "OK";         code = '0'; break;
        case Result::Connect:   text = "CONNECT";    code = '1'; break;
        case Result::NoCarrier: text = "NO CARRIER"; code = '3'; break;
        case Result::Error:     text = "ERROR";      code = '4'; break;
    }
    LOG(Periph, "[Modem] reply %s\n", text);
    if (quiet_) return;
    const char  cr = static_cast<char>(s_[kRegTerminator]);
    const char  lf = static_cast<char>(s_[kRegFormat]);
    std::string out;
    if (verbose_) out.append(1, cr).append(1, lf).append(text).append(1, cr).append(1, lf);
    else          out.append(1, code).append(1, cr);
    PushStr(out);
}

void ModemPersonality::SetCarrier(bool on) {
    online_ = on;
    if (on) {
        if (!terminator_) terminator_ = std::make_unique<PppTerminator>(emu_, *uart_, net_id_);
        terminator_->Start();
        EnterDataMode();
    } else {
        data_mode_        = false;
        dtr_loss_pending_ = false;
        escape_count_     = 0u;
        if (terminator_) terminator_->Stop();
        ArmTimer();
    }
    PushModemInputs();
}

void ModemPersonality::EnterDataMode() {
    data_mode_    = true;
    escape_count_ = 0u;
    last_tx_ns_   = NowNs();
    flow_.EnterDataState();
    UpdateReceiveReady();
}

void ModemPersonality::EnterOnlineCommandState() {
    data_mode_        = false;
    dtr_loss_pending_ = false;
    escape_count_     = 0u;
    line_.clear();
    prefix_        = Prefix::None;
    line_overflow_ = false;
    UpdateReceiveReady();
    ArmTimer();
    Reply(Result::Ok);
}

void ModemPersonality::PushModemInputs() {
    uart_->SetModemInputs(true, true, false, dcd_mode_ == 0u || online_);
}

void ModemPersonality::UpdateReceiveReady() {
    if (terminator_) terminator_->SetReceiveReady(data_mode_ && flow_.ReceiveReady(rts_));
}

void ModemPersonality::PushStr(const std::string& s) {
    uart_->PushRx(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

void ModemPersonality::SaveState(StateWriter& w) {
    s_.SaveState(w);
    w.Write<uint8_t>("modem_echo", echo_ ? 1u : 0u);
    w.Write<uint8_t>("modem_quiet", quiet_ ? 1u : 0u);
    w.Write<uint8_t>("modem_verbose", verbose_ ? 1u : 0u);
    w.Write("modem_dcd_mode", dcd_mode_);
    w.Write("modem_dtr_mode", dtr_mode_);
    w.Write<uint8_t>("modem_prefix", static_cast<uint8_t>(prefix_));
    w.Write<uint8_t>("modem_line_overflow", line_overflow_ ? 1u : 0u);
    w.Write<uint8_t>("modem_dtr", dtr_ ? 1u : 0u);
    w.Write<uint8_t>("modem_rts", rts_ ? 1u : 0u);
    flow_.SaveState(w);
    WriteText(w, "modem_line_count", "modem_line", line_);
    WriteText(w, "modem_last_body_count", "modem_last_body", last_body_);
}

void ModemPersonality::RestoreState(StateReader& r) {
    s_.RestoreState(r);
    uint8_t echo = 0u, quiet = 0u, verbose = 0u, prefix = 0u, overflow = 0u, dtr = 0u, rts = 0u;
    r.Read("modem_echo", echo);
    r.Read("modem_quiet", quiet);
    r.Read("modem_verbose", verbose);
    r.Read("modem_dcd_mode", dcd_mode_);
    r.Read("modem_dtr_mode", dtr_mode_);
    r.Read("modem_prefix", prefix);
    r.Read("modem_line_overflow", overflow);
    r.Read("modem_dtr", dtr);
    r.Read("modem_rts", rts);
    flow_.RestoreState(r);
    dtr_ = dtr != 0u;
    rts_ = rts != 0u;
    prefix_ = static_cast<Prefix>(prefix);
    ReadText(r, "modem_line_count", "modem_line", line_);
    ReadText(r, "modem_last_body_count", "modem_last_body", last_body_);
    echo_          = echo != 0u;
    quiet_         = quiet != 0u;
    verbose_       = verbose != 0u;
    line_overflow_ = overflow != 0u;
}

void ModemPersonality::PostRestore() { SetCarrier(false); }

}

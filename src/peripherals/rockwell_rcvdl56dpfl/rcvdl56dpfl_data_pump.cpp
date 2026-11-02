#include "rcvdl56dpfl_data_pump.h"

#include "rcvdl56dpfl_bit_tick.h"
#include "rcvdl56dpfl_registers.h"

#include <cerfcore/core/byte_order.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

using namespace rcvdl56dpfl_reg;

constexpr uint8_t kBufferControl = kTdbie | kRdbie;

constexpr uint8_t kDownloadByteMode = 79u;
constexpr uint8_t kDownloadWordMode = 95u;
constexpr uint8_t kDownloadReady    = 0xFFu;
constexpr uint8_t kDownloadChecksum = 0x00u;
constexpr uint8_t kNoDtmfReported   = 0x00u;
constexpr uint8_t kConfDialing      = 0x81u;

constexpr Rcvdl56dpflConfiguration::Registers kPowerOnDefaults = {
    0x00u, 0x00u, 0x80u, 0x10u, 0x00u, 0x08u, 0x00u, 0x80u,
    0x00u, 0x24u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x76u, 0x90u, 0x00u, 0x30u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x08u, 0x00u,
};

}

Rcvdl56dpflDataPump::Rcvdl56dpflDataPump(Rcvdl56dpflDataPumpHost& host) : host_(host) {
    ram_.PowerOn();
    line_.Reset(kPowerOnDefaults);
}

uint8_t Rcvdl56dpflDataPump::Read(uint32_t reg) {
    if (reset_held_) return line_.Regs()[reg];
    CatchUp();
    const uint8_t value = line_.Regs()[reg];
    if (reg == kRbuffer) {
        line_.HostReadReceiveBuffer();
        RearmTimer();
    }
    PublishIrq();
    return value;
}

const char* Rcvdl56dpflDataPump::Write(uint32_t reg, uint8_t value) {
    if (reset_held_) {
        line_.Regs()[reg] = value;
        PublishIrq();
        return nullptr;
    }
    CatchUp();
    const char* refused = WriteRegister(reg, value);
    if (refused == nullptr) {
        line_.RefreshLine();
        SyncBitRate();
        RearmTimer();
    }
    PublishIrq();
    return refused;
}

void Rcvdl56dpflDataPump::TimerElapsed() {
    CatchUp();
    RearmTimer();
    PublishIrq();
}

void Rcvdl56dpflDataPump::RearmTimer() {
    host_.ArmTimer(reset_held_ ? kRcvdl56dpflNoEvent : line_.InterruptRiseTick());
}

void Rcvdl56dpflDataPump::CatchUp() {
    const uint64_t elapsed = host_.BitTicksElapsed();
    line_.CatchUp(elapsed, host_.BitTickNow());
}

void Rcvdl56dpflDataPump::SyncBitRate() {
    if (line_.CarrierWanted()) host_.SetBitRate(Rcvdl56dpflConfiguration::BitRate(line_.ActiveConf()));
}

const char* Rcvdl56dpflDataPump::WriteRegister(uint32_t reg, uint8_t value) {
    if ((value ^ line_.Regs()[reg]) & Rcvdl56dpflConfiguration::UnmodeledBits(reg))
        return "a control or reserved bit with no model changed";
    switch (reg) {
        case kRbuffer:   return "RBUFFER write";
        case kNewStatus: return WriteNewStatusRegister(value);
        case kMemAccess: return WriteMemoryAccessRegister(value);
        case kBuffer:    WriteBufferStatusRegister(value); return nullptr;
        case kTbuffer:   return WriteTransmitBuffer(value);
        case kDownload:  WriteDownloadControl(value);      return nullptr;
        case kMode:      WriteModeRegister(value);         return nullptr;
        default:
            line_.Regs()[reg] = value;
            return nullptr;
    }
}

void Rcvdl56dpflDataPump::PowerOn() {
    reset_held_  = false;
    downloading_ = false;
    ram_.PowerOn();
    line_.Reset(kPowerOnDefaults);
    RearmTimer();
}

void Rcvdl56dpflDataPump::SetResetPin(bool high) {
    if (!high) {
        reset_held_ = true;
        RearmTimer();
        return;
    }
    if (!reset_held_) return;
    PowerOn();
    PublishIrq();
}

void Rcvdl56dpflDataPump::PublishIrq() {
    const bool level = line_.IrqAsserted();
    if (level == irq_level_) return;
    irq_level_ = level;
    host_.SetIrqLevel(level);
}

void Rcvdl56dpflDataPump::SaveState(StateWriter& w) const {
    line_.SaveState(w);
    ram_.SaveState(w);
    w.Write<uint8_t>("reset_held", reset_held_ ? 1u : 0u);
    w.Write<uint8_t>("downloading", downloading_ ? 1u : 0u);
}

void Rcvdl56dpflDataPump::RestoreState(StateReader& r) {
    line_.RestoreState(r);
    ram_.RestoreState(r);
    uint8_t held = 0u;
    r.Read("reset_held", held);
    reset_held_ = held != 0u;
    uint8_t downloading = 0u;
    r.Read("downloading", downloading);
    downloading_ = downloading != 0u;
    irq_level_   = line_.IrqAsserted();
}

const char* Rcvdl56dpflDataPump::WriteNewStatusRegister(uint8_t value) {
    const uint8_t old = line_.Regs()[kNewStatus];
    uint8_t next = static_cast<uint8_t>((old & (kNsia | kNcia)) | (value & ~(kNsia | kNcia)));
    if ((old & kNews) == 0u) next &= static_cast<uint8_t>(~kNews);
    if ((next & kNcie) == 0u) next &= static_cast<uint8_t>(~kNcia);
    line_.Regs()[kNewStatus] = next;
    line_.UpdateNewStatusInterrupt();
    if (next & kNewc) return ProcessNewConfiguration();
    return nullptr;
}

void Rcvdl56dpflDataPump::WriteBufferStatusRegister(uint8_t value) {
    uint8_t& buffer = line_.Regs()[kBuffer];
    buffer = static_cast<uint8_t>((buffer & ~kBufferControl) | (value & kBufferControl));
    line_.UpdateBufferInterrupts();
}

const char* Rcvdl56dpflDataPump::WriteMemoryAccessRegister(uint8_t value) {
    if (value & kMemcr) return "MEMCR (continuous RAM read) set";
    line_.Regs()[kMemAccess] = value;
    if (value & kMeacc) ProcessRamAccess();
    return nullptr;
}

const char* Rcvdl56dpflDataPump::WriteTransmitBuffer(uint8_t value) {
    line_.Regs()[kTbuffer] = value;
    const bool taken_at_once = downloading_ || line_.ActiveConf() == kConfDialing;
    if (!taken_at_once)
        if (const char* refused = line_.QueueTransmitByte(value)) return refused;
    line_.SetTransmitBufferEmpty(false);
    PublishIrq();
    if (taken_at_once) line_.SetTransmitBufferEmpty(true);
    return nullptr;
}

void Rcvdl56dpflDataPump::WriteDownloadControl(uint8_t value) {
    line_.Regs()[kDownload] = value;
    if (!downloading_ && (value == kDownloadByteMode || value == kDownloadWordMode)) {
        downloading_ = true;
        line_.SetStatus(kDtmfStatus, value);
        line_.SetStatus(kSecrxb, kDownloadReady);
        return;
    }
    if (downloading_ && value == 0u) {
        downloading_ = false;
        line_.SetStatus(kDtmfStatus, kNoDtmfReported);
        line_.SetStatus(kSectxb, kDownloadReady);
        line_.SetStatus(kSecrxb, kDownloadChecksum);
    }
}

void Rcvdl56dpflDataPump::WriteModeRegister(uint8_t value) {
    uint8_t& mode = line_.Regs()[kMode];
    const bool dtr_fell = (mode & kDtr) && !(value & kDtr);
    mode = value;
    if (dtr_fell) ram_.ApplyDtrOff();
}

const char* Rcvdl56dpflDataPump::ProcessNewConfiguration() {
    auto& regs = line_.Regs();
    if (regs[kSecondary] & kSfres) {
        PowerOn();
        return nullptr;
    }
    if (regs[kMode] & kLeasedLine) return "NEWC with LL (leased line) set";
    if (regs[kPower] & kSleep) return "NEWC with SLEEP set";
    const uint8_t txclk = regs[kControl13] & kTxclkMask;
    if (txclk != kTxclkInternal && txclk != kTxclkSlave) return "NEWC with TXCLK (13h:1-0) external or reserved";
    line_.LatchConfiguration();
    ram_.ApplyNewConfiguration(line_.ActiveConf(), (regs[kControl03] & kGts) != 0u, (regs[kControl05] & kCeq) != 0u);
    regs[kNewStatus] &= static_cast<uint8_t>(~kNewc);
    if (regs[kNewStatus] & kNcie) regs[kNewStatus] |= kNcia;
    return nullptr;
}

void Rcvdl56dpflDataPump::ProcessRamAccess() {
    auto& regs = line_.Regs();
    const uint32_t code = RamCode();
    if (regs[kMemAccess] & kMemw)
        ram_.Write(code, cerf::le::U16(regs.data(), kMedal));
    else
        cerf::le::Put16(regs.data() + kMedal, ram_.Read(code));
    regs[kMemAccess] &= static_cast<uint8_t>(~kMeacc);
    if (!ram_.MemoryAccessNewsMasked()) line_.NoteNewStatus();
}

uint32_t Rcvdl56dpflDataPump::RamCode() const {
    const auto&    regs = line_.Regs();
    const uint32_t high = regs[kMemAccess] & kMeaddhMask;
    const uint32_t ext  = (regs[kMemAccess] & kMeaddExt) ? 0x1000u : 0u;
    return ext | (high << 8) | regs[kMeaddl];
}

}

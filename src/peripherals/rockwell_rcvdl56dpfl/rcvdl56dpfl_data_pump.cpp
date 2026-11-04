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

uint8_t HostNewStatusByte(uint8_t old, uint8_t value) {
    uint8_t next = static_cast<uint8_t>((old & (kNsia | kNcia)) | (value & ~(kNsia | kNcia)));
    if ((old & kNews) == 0u) next &= static_cast<uint8_t>(~kNews);
    return next;
}

uint8_t HostBufferStatusByte(uint8_t old, uint8_t value) {
    return static_cast<uint8_t>((old & ~kBufferControl) | (value & kBufferControl));
}

constexpr int64_t  kSelfTestWindowNs     = 5'000'000;
constexpr uint32_t kSelfTestRegisters[]  = {0x00u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u,
                                            0x17u, 0x18u, 0x19u, 0x1Au, 0x1Bu, 0x1Cu, 0x1Du};

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
    AdvanceSelfTest();
    if (InSelfTest()) {
        const uint8_t value = line_.Regs()[reg];
        if (reg == kTbuffer) EndSelfTestWindow(host_.GuestNs());
        PublishIrq();
        return value;
    }
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
    AdvanceSelfTest();
    if (InSelfTest()) {
        const char* refused = WriteDuringSelfTest(reg, value);
        PublishIrq();
        return refused;
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
    StartSelfTest();
    RearmTimer();
}

void Rcvdl56dpflDataPump::StartSelfTest() {
    const Rcvdl56dpflSelfTest& t = *self_test_;
    LoadSelfTestWord(0x1Du, 0x1Cu, t.ram1_checksum);
    LoadSelfTestWord(0x1Bu, 0x1Au, t.ram2_checksum);
    LoadSelfTestWord(0x19u, 0x18u, t.rom1_checksum);
    LoadSelfTestWord(0x17u, 0x16u, t.rom2_checksum);
    LoadSelfTestWord(0x15u, 0x14u, t.timer_rom_ram);
    LoadSelfTestWord(0x13u, 0x12u, t.controller_part_number);
    LoadSelfTestWord(0x11u, 0x00u, t.controller_revision);
    line_.Regs()[kBuffer] |= kTdbe;
    self_test_phase_  = SelfTestPhase::kController;
    self_test_end_ns_ = host_.GuestNs() + kSelfTestWindowNs;
}

void Rcvdl56dpflDataPump::AdvanceSelfTest() {
    if (!InSelfTest()) return;
    const int64_t now = host_.GuestNs();
    while (InSelfTest() && now >= self_test_end_ns_) EndSelfTestWindow(self_test_end_ns_);
}

void Rcvdl56dpflDataPump::EndSelfTestWindow(int64_t at_ns) {
    if (self_test_phase_ == SelfTestPhase::kController) {
        const Rcvdl56dpflSelfTest& t = *self_test_;
        for (const uint32_t reg : kSelfTestRegisters) line_.Regs()[reg] = 0u;
        LoadSelfTestWord(0x1Bu, 0x1Au, t.ec_checksum);
        LoadSelfTestWord(0x19u, 0x18u, t.multiplier_checksum);
        LoadSelfTestWord(0x17u, 0x16u, t.dsp_ram_checksum);
        LoadSelfTestWord(0x15u, 0x14u, t.dsp_rom_checksum);
        LoadSelfTestWord(0x13u, 0x12u, t.dsp_part_number);
        LoadSelfTestWord(0x11u, 0x00u, t.dsp_revision);
        line_.Regs()[kBuffer] |= kTdbe;
        self_test_phase_  = SelfTestPhase::kDsp;
        self_test_end_ns_ = at_ns + kSelfTestWindowNs;
        return;
    }
    self_test_phase_ = SelfTestPhase::kNone;
    line_.Reset(kPowerOnDefaults);
    RearmTimer();
}

void Rcvdl56dpflDataPump::LoadSelfTestWord(uint32_t high_reg, uint32_t low_reg, uint16_t value) {
    line_.Regs()[high_reg] = static_cast<uint8_t>(value >> 8);
    line_.Regs()[low_reg]  = static_cast<uint8_t>(value);
}

void Rcvdl56dpflDataPump::SetResetPin(bool high) {
    if (!high) {
        reset_held_      = true;
        self_test_phase_ = SelfTestPhase::kNone;
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
    w.Write<uint8_t>("self_test_phase", static_cast<uint8_t>(self_test_phase_));
    w.Write<int64_t>("self_test_remaining_ns", InSelfTest() ? self_test_end_ns_ - host_.GuestNs() : 0);
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
    uint8_t phase = 0u;
    r.Read("self_test_phase", phase);
    self_test_phase_ = static_cast<SelfTestPhase>(phase);
    int64_t remaining_ns = 0;
    r.Read("self_test_remaining_ns", remaining_ns);
    self_test_end_ns_ = host_.GuestNs() + remaining_ns;
    irq_level_   = line_.IrqAsserted();
}

const char* Rcvdl56dpflDataPump::WriteDuringSelfTest(uint32_t reg, uint8_t value) {
    auto& regs = line_.Regs();
    switch (reg) {
        case kRbuffer:
            return "RBUFFER write";
        case kTbuffer:
            regs[kTbuffer] = value;
            regs[kBuffer] &= static_cast<uint8_t>(~(kTdbe | kTdbia));
            EndSelfTestWindow(host_.GuestNs());
            return nullptr;
        case kBuffer:
            regs[kBuffer] = HostBufferStatusByte(regs[kBuffer], value);
            return nullptr;
        case kNewStatus:
            regs[kNewStatus] = HostNewStatusByte(regs[kNewStatus], value);
            return nullptr;
        default:
            regs[reg] = value;
            return nullptr;
    }
}

const char* Rcvdl56dpflDataPump::WriteNewStatusRegister(uint8_t value) {
    uint8_t next = HostNewStatusByte(line_.Regs()[kNewStatus], value);
    if ((next & kNcie) == 0u) next &= static_cast<uint8_t>(~kNcia);
    line_.Regs()[kNewStatus] = next;
    line_.UpdateNewStatusInterrupt();
    if (next & kNewc) return ProcessNewConfiguration();
    return nullptr;
}

void Rcvdl56dpflDataPump::WriteBufferStatusRegister(uint8_t value) {
    uint8_t& buffer = line_.Regs()[kBuffer];
    buffer = HostBufferStatusByte(buffer, value);
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
        line_.Regs()[kNewStatus] |= kNewc;
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

#include "sh7707_pcc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../sh77xx/sh77xx_intc.h"
#include "../sh77xx/sh77xx_pfc.h"
#include "sh7707_id.h"
#include "sh7707_pcc_sockets.h"

#include <iterator>

namespace cerfcore {

REGISTER_SERVICE(Sh7707Pcc);

namespace {

constexpr uint8_t kPccModuleStop = 0x04u;

constexpr uint32_t kInputPort[2] = {kSh77xxPortG, kSh77xxPortF};

constexpr uint8_t kPinWriteProtect = 0x80u;
constexpr uint8_t kPinReady        = 0x40u;
constexpr uint8_t kPinBvd1         = 0x20u;
constexpr uint8_t kPinBvd2         = 0x10u;
constexpr uint8_t kPinCd1          = 0x08u;
constexpr uint8_t kPinCd2          = 0x04u;
constexpr uint8_t kPinVs1          = 0x02u;
constexpr uint8_t kPinVs2          = 0x01u;
constexpr uint8_t kCardDetectPins  = kPinCd1 | kPinCd2;
constexpr uint8_t kVoltageSense    = kPinVs1 | kPinVs2;
constexpr uint8_t kAllPins         = 0xFFu;

constexpr uint32_t kPcc0Causes = 7u;

constexpr uint32_t kPcc1CauseBits[5] = {HitachiPcc::kCauseBatteryDead, HitachiPcc::kCauseBatteryWarning,
                                        HitachiPcc::kCauseReadyChange, HitachiPcc::kCauseDetectChange,
                                        HitachiPcc::kCauseSoftwareDetect};

Sh77xxIntcSource SourceAfter(Sh77xxIntcSource first, uint32_t step) {
    return static_cast<Sh77xxIntcSource>(static_cast<uint32_t>(first) + step);
}

uint8_t PackPins(const HitachiPccSocketPins& p) {
    return static_cast<uint8_t>((p.write_protect ? kPinWriteProtect : 0u) | (p.ready ? kPinReady : 0u) |
                                (p.bvd1 ? kPinBvd1 : 0u) | (p.bvd2 ? kPinBvd2 : 0u) | (p.vs1 ? kPinVs1 : 0u) |
                                (p.vs2 ? kPinVs2 : 0u));
}

void UnpackPins(uint8_t bits, HitachiPccSocketPins& p) {
    p.write_protect = (bits & kPinWriteProtect) != 0u;
    p.ready         = (bits & kPinReady) != 0u;
    p.bvd1          = (bits & kPinBvd1) != 0u;
    p.bvd2          = (bits & kPinBvd2) != 0u;
    p.vs1           = (bits & kPinVs1) != 0u;
    p.vs2           = (bits & kPinVs2) != 0u;
}

}

bool Sh7707Pcc::ShouldRegister() { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }

void Sh7707Pcc::OnReady() {
    intc_    = &emu_.Get<Sh77xxIntc>();
    pfc_     = &emu_.Get<Sh77xxPfc>();
    stbcr2_  = &emu_.Get<Sh77xxStbcr2>();
    sockets_ = &emu_.Get<Sh7707PccSockets>();
    {
        std::lock_guard<std::mutex> lk(lock_);
        for (uint32_t ch = 0; ch < 2u; ++ch) held_[ch] = PinsLocked(ch);
    }
    HitachiPccBase::OnReady();
    pfc_->AddControlListener([this] { TrackPinFunctions(); });
    emu_.Get<GuestCpuReset>().RegisterResetReleaseListener([this] { TrackPinFunctions(); });
    TrackPinFunctions();
}

uint8_t Sh7707Pcc::ReadByte(uint32_t addr) {
    RequireClock("ReadByte", addr, 0u);
    return HitachiPccBase::ReadByte(addr);
}

void Sh7707Pcc::WriteByte(uint32_t addr, uint8_t value) {
    RequireClock("WriteByte", addr, value);
    HitachiPccBase::WriteByte(addr, value);
}

uint16_t Sh7707Pcc::ReadHalf(uint32_t addr) { HaltUnsupportedAccess("ReadHalf", addr, 0u); }

void Sh7707Pcc::WriteHalf(uint32_t addr, uint16_t value) { HaltUnsupportedAccess("WriteHalf", addr, value); }

void Sh7707Pcc::SetSocketPower(uint32_t channel, bool on) {
    {
        std::lock_guard<std::mutex> lk(lock_);
        if (power_[channel] == on) return;
        power_[channel] = on;
    }
    ApplySocketDrive();
}

void Sh7707Pcc::RequireClock(const char* access, uint32_t addr, uint32_t value) const {
    if (!stbcr2_->ModuleStopped(kPccModuleStop)) return;
    emu_.Get<Fatal>().Die("Sh7707Pcc: %s of 0x%02X at 0x%08X while STBCR2 MSTP5 stops the PCC clock", access, value,
                          addr);
}

uint8_t Sh7707Pcc::RoutedPins(uint32_t port) const {
    uint8_t routed = 0u;
    for (uint32_t pin = 0; pin < kSh77xxPortPins; ++pin) {
        if (pfc_->PinMode(port, pin) == kSh77xxPinModeOther) routed = static_cast<uint8_t>(routed | (1u << pin));
    }
    return routed;
}

void Sh7707Pcc::ReadPinFunctions(uint8_t (&inputs)[2], uint8_t& outputs) const {
    for (uint32_t ch = 0; ch < 2u; ++ch) inputs[ch] = RoutedPins(kInputPort[ch]);
    outputs = RoutedPins(kSh77xxPortE);
}

void Sh7707Pcc::TrackPinFunctions() {
    uint8_t inputs[2] = {};
    uint8_t outputs   = 0u;
    ReadPinFunctions(inputs, outputs);
    std::lock_guard<std::mutex> lk(lock_);
    for (uint32_t ch = 0; ch < 2u; ++ch) {
        held_[ch]          = PresentLocked(ch, PinsLocked(ch));
        routed_inputs_[ch] = inputs[ch];
    }
    routed_outputs_ = outputs;
    for (uint32_t ch = 0; ch < 2u; ++ch) RefreshChannelLocked(ch);
}

void Sh7707Pcc::SaveState(StateWriter& w) {
    HitachiPccBase::SaveState(w);
    std::lock_guard<std::mutex> lk(lock_);
    for (uint32_t ch = 0; ch < 2u; ++ch) w.Write<uint8_t>("held_pins", PackPins(held_[ch]));
}

void Sh7707Pcc::RestoreState(StateReader& r) {
    HitachiPccBase::RestoreState(r);
    std::lock_guard<std::mutex> lk(lock_);
    for (uint32_t ch = 0; ch < 2u; ++ch) {
        uint8_t bits = 0u;
        r.Read("held_pins", bits);
        UnpackPins(bits, held_[ch]);
        power_[ch] = SlotPowered(ch);
    }
}

void Sh7707Pcc::PostRestore() {
    uint8_t inputs[2] = {};
    uint8_t outputs   = 0u;
    ReadPinFunctions(inputs, outputs);
    {
        std::lock_guard<std::mutex> lk(lock_);
        routed_inputs_[0] = inputs[0];
        routed_inputs_[1] = inputs[1];
        routed_outputs_   = outputs;
    }
    HitachiPccBase::PostRestore();
}

ObjectIdentity Sh7707Pcc::SocketIdentity(uint32_t ch) const { return sockets_->Socket(ch); }

bool Sh7707Pcc::SocketFitted(uint32_t ch) const { return sockets_->Fitted(ch); }

bool Sh7707Pcc::SocketPoweredLocked(uint32_t ch) const { return power_[ch]; }

bool Sh7707Pcc::WriteProtectedLocked(uint32_t ch) const {
    (void)ch;
    return false;
}

HitachiPccSocketPins Sh7707Pcc::PresentLocked(uint32_t ch, const HitachiPccSocketPins& live) const {
    HitachiPccSocketPins p = live;
    UnpackPins(static_cast<uint8_t>((PackPins(live) & routed_inputs_[ch]) | (PackPins(held_[ch]) & ~routed_inputs_[ch])),
               p);
    return p;
}

void Sh7707Pcc::DeliverCausesLocked(uint32_t ch, uint32_t causes) {
    if (ch == 0u) {
        for (uint32_t bit = 0; bit < kPcc0Causes; ++bit) {
            intc_->SetSourceRequest(SourceAfter(Sh77xxIntcSource::kPc0Bdi, bit), ((causes >> bit) & 1u) != 0u);
        }
        return;
    }
    for (uint32_t k = 0; k < std::size(kPcc1CauseBits); ++k) {
        intc_->SetSourceRequest(SourceAfter(Sh77xxIntcSource::kPc1Bdi, k), (causes & kPcc1CauseBits[k]) != 0u);
    }
}

void Sh7707Pcc::RequireCardCycle(uint32_t ch, uint32_t offset) {
    if (stbcr2_->ModuleStopped(kPccModuleStop)) {
        emu_.Get<Fatal>().Die("Sh7707Pcc: card cycle at offset 0x%08X to PCC%u while STBCR2 MSTP5 stops the PCC clock",
                              offset, ch);
    }
    if (routed_outputs_ != kAllPins) {
        emu_.Get<Fatal>().Die("Sh7707Pcc: card cycle at offset 0x%08X to PCC%u with PECR pins 0x%02X outside the "
                              "PC card function", offset, ch, static_cast<uint8_t>(~routed_outputs_));
    }
}

void Sh7707Pcc::BeforeStore(uint32_t i, uint16_t value) {
    if (i != hitachi_pcc_detail::kCscier) return;
    if (((StoredReg(i) ^ value) & hitachi_pcc_detail::kCscierIreqModeMask) == 0u) return;
    const uint32_t cscr = hitachi_pcc_detail::kCscr;
    SetStoredReg(cscr, static_cast<uint16_t>(StoredReg(cscr) & ~hitachi_pcc_detail::kCscrIreq));
}

uint16_t Sh7707Pcc::SampleRegister(uint32_t i, uint32_t addr) {
    const uint32_t ch = i / hitachi_pcc_detail::kChannelRegs;
    const uint8_t covered = static_cast<uint8_t>(routed_inputs_[ch] | kCardDetectPins | kVoltageSense);
    if (covered != kAllPins) {
        emu_.Get<Fatal>().Die("Sh7707Pcc: read at 0x%08X with PCC%u input pins 0x%02X outside the PC card function",
                              addr, ch, static_cast<uint8_t>(~covered));
    }
    return HitachiPccBase::SampleRegister(i, addr);
}

}

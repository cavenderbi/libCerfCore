#include "sh77xx_adc.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "sh77xx_analog_inputs.h"
#include "sh77xx_pfc.h"
#include "sh77xx_standby_entry.h"

namespace cerfcore {

namespace {

constexpr uint32_t kOffControlStatus = 0x10u;
constexpr uint32_t kOffControl       = 0x12u;
constexpr uint32_t kDataStride       = 4u;
constexpr uint32_t kDataLowByte      = 2u;

constexpr uint8_t kAdf          = 0x80u;
constexpr uint8_t kAdie         = 0x40u;
constexpr uint8_t kAdst         = 0x20u;
constexpr uint8_t kMulti        = 0x10u;
constexpr uint8_t kCks          = 0x08u;
constexpr uint8_t kChannel      = 0x07u;
constexpr uint8_t kGroup        = 0x04u;
constexpr uint8_t kGroupChannel = 0x03u;
constexpr uint8_t kStoredStatus = 0x5Fu;
constexpr uint8_t kSelection    = kMulti | kCks | kChannel;

constexpr uint8_t kSingleChannel0Selection = 0x00u;

constexpr uint64_t kRepeatConversionStates     = 256u;
constexpr uint64_t kRepeatConversionStatesFast = 128u;

constexpr uint8_t kTriggerEnable = 0xC0u;

constexpr const char* kDataNames[4] = {"addra", "addrb", "addrc", "addrd"};

}

void Sh77xxAdc::OnReady() {
    intc_   = &emu_.Get<Sh77xxIntc>();
    dmac_   = &emu_.Get<Sh77xxDmac>();
    pfc_    = &emu_.Get<Sh77xxPfc>();
    stbcr2_ = &emu_.Get<Sh77xxStbcr2>();
    inputs_ = &emu_.Get<Sh77xxAnalogInputs>();
    clock_  = &emu_.Get<GuestCycleClock>();
    event_  = clock_->Add([this] {
        Sync();
        Drive();
    });
    pclk_.Attach([this] { Arm(); });
    emu_.Get<Sh77xxStandbyEntry>().RegisterListener([this] { Initialize(); });
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Initialize(); });
    Initialize();
    emu_.Get<PeripheralDispatcher>().Register(this);
}

uint8_t Sh77xxAdc::ReadByte(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (off < kSh77xxAdcDataBytes && (off & 1u) == 0u) return ReadData(off);
    if (off == kOffControlStatus) return ReadControlStatus();
    if (off == kOffControl) return static_cast<uint8_t>(adcr_ | Control().read_ones);
    HaltUnsupportedAccess("ReadByte", addr, 0u);
}

void Sh77xxAdc::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off = addr - MmioBase();
    if (off == kOffControlStatus) {
        WriteControlStatus(value);
        return;
    }
    if (off == kOffControl) {
        if ((value & Control().write_zero) != 0u) HaltUnsupportedAccess("WriteByte", addr, value);
        if (scanning_ && ((value ^ adcr_) & Control().scan) != 0u) {
            emu_.Get<Fatal>().Die("Sh77xxAdc: ADCR 0x%02X changes the scan selection during a scan with ADCR 0x%02X", value,
                                  adcr_);
        }
        adcr_ = static_cast<uint8_t>(value & Control().stored);
        return;
    }
    HaltUnsupportedAccess("WriteByte", addr, value);
}

uint8_t Sh77xxAdc::ReadData(uint32_t offset) {
    Sync();
    const uint16_t data = data_[offset / kDataStride];
    uint8_t        value;
    if ((offset & kDataLowByte) == 0u) {
        value        = static_cast<uint8_t>(data >> 2);
        temp_        = static_cast<uint8_t>((data & 0x3u) << 6);
        temp_loaded_ = true;
    } else {
        if (!temp_loaded_) {
            emu_.Get<Fatal>().Die("Sh77xxAdc: ADDR%c low byte read before any high byte read",
                                  static_cast<int>('A' + offset / kDataStride));
        }
        value = temp_;
    }
    if (dmac_->Transferring(Sh77xxDmaRequest::kAd)) {
        ClearEndFlag();
        Drive();
    }
    return value;
}

uint8_t Sh77xxAdc::ReadControlStatus() {
    Sync();
    if (end_) end_read_ = true;
    return static_cast<uint8_t>((end_ ? kAdf : 0u) | control_ | (scanning_ ? kAdst : 0u));
}

void Sh77xxAdc::WriteControlStatus(uint8_t value) {
    Sync();
    if ((value & kAdf) == 0u && end_read_) ClearEndFlag();
    const uint8_t control = static_cast<uint8_t>(value & kStoredStatus);
    const bool    adst    = (value & kAdst) != 0u;
    if (scanning_ && adst && ((control ^ control_) & kSelection) != 0u &&
        (control & kSelection) != kSingleChannel0Selection) {
        emu_.Get<Fatal>().Die("Sh77xxAdc: ADCSR 0x%02X changes the conversion selection during a scan with ADCSR 0x%02X",
                              value, control_ | kAdst);
    }
    if (scanning_ && (!adst || ((control ^ control_) & kSelection) != 0u)) StopScan();
    control_ = control;
    if (adst && !scanning_) Start();
    Drive();
}

bool Sh77xxAdc::ExternalTrigger() {
    const TriggerPin pin = AdTriggerPin();
    if (pfc_->PinMode(pin.port, pin.pin) != kSh77xxPinModeOther) return false;
    if ((adcr_ & kTriggerEnable) != kTriggerEnable || scanning_) return false;
    Start();
    Drive();
    return true;
}

void Sh77xxAdc::Start() {
    RequireClock();
    Convert();
    if (!Scans()) return;
    scanning_  = true;
    next_pass_ = pclk_.DividedTicks(0u) + PassTicks();
}

void Sh77xxAdc::Convert() {
    const uint32_t channel = control_ & kChannel;
    if ((control_ & kMulti) != 0u) {
        const uint32_t first = channel & kGroup;
        for (uint32_t i = 0u; i <= (channel & kGroupChannel); ++i) data_[i] = inputs_->Convert(first + i);
    } else {
        data_[channel & kGroupChannel] = inputs_->Convert(channel);
    }
    end_ = true;
}

void Sh77xxAdc::RequireClock() const {
    if (stbcr2_->ModuleStopped(Stbcr2ModuleStop())) {
        emu_.Get<Fatal>().Die("Sh77xxAdc: conversion with ADCSR 0x%02X while STBCR2 stops the A/D clock",
                              control_ | kAdst);
    }
}

bool Sh77xxAdc::Scans() const {
    if ((control_ & kMulti) == 0u) return false;
    return Control().multi_scans || (adcr_ & Control().scan) != 0u;
}

uint64_t Sh77xxAdc::PassTicks() const {
    const uint64_t channels = (control_ & kGroupChannel) + 1u;
    return channels * ((control_ & kCks) != 0u ? kRepeatConversionStatesFast : kRepeatConversionStates);
}

void Sh77xxAdc::Sync() {
    if (!scanning_) return;
    const uint64_t now = pclk_.DividedTicks(0u);
    if (now < next_pass_) return;
    RequireClock();
    const uint64_t period = PassTicks();
    next_pass_ += ((now - next_pass_) / period + 1u) * period;
    Convert();
}

void Sh77xxAdc::Arm() {
    if (!scanning_ || end_ || (control_ & kAdie) == 0u) {
        clock_->Disarm(event_);
        return;
    }
    clock_->Arm(event_, pclk_.CycleOfDividedTick(next_pass_, 0u));
}

void Sh77xxAdc::StopScan() { scanning_ = false; }

void Sh77xxAdc::ClearEndFlag() {
    end_      = false;
    end_read_ = false;
}

bool Sh77xxAdc::Interrupting() const { return end_ && (control_ & kAdie) != 0u; }

void Sh77xxAdc::Drive() {
    Arm();
    if (Interrupting()) {
        dmac_->Request(Sh77xxDmaRequest::kAd);
    } else if (!dmac_->Transferring(Sh77xxDmaRequest::kAd)) {
        dmac_->Withdraw(Sh77xxDmaRequest::kAd);
    }
    intc_->SetSourceRequest(Sh77xxIntcSource::kAdi,
                            Interrupting() && !dmac_->TakesRequest(Sh77xxDmaRequest::kAd));
}

void Sh77xxAdc::Initialize() {
    for (uint16_t& data : data_) data = 0u;
    control_     = 0u;
    adcr_        = 0u;
    temp_loaded_ = false;
    ClearEndFlag();
    StopScan();
    Drive();
}

void Sh77xxAdc::SaveState(StateWriter& w) {
    for (uint32_t i = 0u; i < kDataRegisters; ++i) w.Write(kDataNames[i], data_[i]);
    w.Write("control", control_);
    w.Write("adcr", adcr_);
    w.Write("temp", temp_);
    w.Write("end", end_);
    w.Write("end_read", end_read_);
    w.Write("temp_loaded", temp_loaded_);
    const uint64_t now = scanning_ ? pclk_.DividedTicks(0u) : 0u;
    w.Write("scanning", scanning_);
    w.Write("ticks_to_pass", scanning_ && now < next_pass_ ? next_pass_ - now : uint64_t{0u});
    pclk_.Save(w);
}

void Sh77xxAdc::RestoreState(StateReader& r) {
    for (uint32_t i = 0u; i < kDataRegisters; ++i) r.Read(kDataNames[i], data_[i]);
    r.Read("control", control_);
    r.Read("adcr", adcr_);
    r.Read("temp", temp_);
    r.Read("end", end_);
    r.Read("end_read", end_read_);
    r.Read("temp_loaded", temp_loaded_);
    r.Read("scanning", scanning_);
    r.Read("ticks_to_pass", restored_ticks_to_pass_);
    pclk_.Restore(r);
}

void Sh77xxAdc::PostRestore() {
    pclk_.PostRestore();
    if (scanning_) next_pass_ = pclk_.DividedTicks(0u) + restored_ticks_to_pass_;
    Drive();
}

}

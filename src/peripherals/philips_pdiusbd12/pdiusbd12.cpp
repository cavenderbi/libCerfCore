#include "pdiusbd12.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../peripheral_dispatcher.h"
#include "pdiusbd12_config.h"

#include <algorithm>

namespace cerfcore {

namespace {

constexpr uint8_t kSetAddressEnable   = 0xD0u;
constexpr uint8_t kSetEndpointEnable  = 0xD8u;
constexpr uint8_t kSetMode            = 0xF3u;
constexpr uint8_t kSetDma             = 0xFBu;
constexpr uint8_t kReadInterrupt      = 0xF4u;
constexpr uint8_t kEndpointStatusBase = 0x40u;
constexpr uint8_t kBuffer             = 0xF0u;
constexpr uint8_t kClearBuffer        = 0xF2u;
constexpr uint8_t kValidateBuffer     = 0xFAu;
constexpr uint8_t kReadChipId         = 0xFDu;

constexpr size_t kEndpointCount      = 6u;
constexpr size_t kMainOut            = 4u;
constexpr size_t kMainIn             = 5u;
constexpr size_t kBufferHeaderBytes  = 2u;
constexpr size_t kGenericPacketBytes = 16u;
constexpr size_t kMainPacketBytes    = 64u;
constexpr size_t kIsoPacketBytes     = 128u;

constexpr uint8_t  kPowerOnConfiguration   = 0x0Eu;
constexpr uint8_t  kPowerOnClockDivision   = 0x0Bu;
constexpr uint8_t  kEndpointConfigShift    = 6u;
constexpr uint8_t  kModeIsoOut             = 1u;
constexpr uint8_t  kModeIsoIn              = 2u;
constexpr uint8_t  kConfigurationReserved  = 0x21u;
constexpr uint8_t  kClockDivisionReserved  = 0x30u;
constexpr uint8_t  kSofOnlyInterrupts      = 0x80u;
constexpr uint8_t  kDmaEnable              = 0x04u;
constexpr uint8_t  kMainInInterruptEnable  = 0x80u;
constexpr uint8_t  kGenericEndpointsEnable = 0x01u;
constexpr uint8_t  kStall                  = 0x01u;
constexpr uint8_t  kSelectFull             = 0x01u;
constexpr uint8_t  kSelectStall            = 0x02u;
constexpr uint16_t kSuspendChange          = 0x0080u;
constexpr uint16_t kClearedOnRead          = 0xFFC0u;
constexpr int64_t  kSuspendAfterNs         = 3000000;
constexpr std::array<uint8_t, 2> kChipId   = {0x12u, 0x10u};

bool IsIn(size_t index) { return (index & 1u) != 0u; }

uint16_t EndpointInterrupt(size_t index) { return static_cast<uint16_t>(1u << index); }

}

REGISTER_SERVICE(Pdiusbd12);

bool Pdiusbd12::ShouldRegister() { return emu_.TryGet<Pdiusbd12Config>() != nullptr; }

void Pdiusbd12::OnReady() {
    config_        = &emu_.Get<Pdiusbd12Config>();
    clock_         = &emu_.Get<ExternalDeviceClock>();
    suspend_timer_ = &clock_->AddTimer([this] { EnterSuspend(); });
    emu_.Get<GuestDeepSleep>().RegisterParkWakeDue([this] { return ParkWakeDueNs(); });
    base_ = std::min(config_->DataPortPa(), config_->CommandPortPa());
    size_ = std::max(config_->DataPortPa(), config_->CommandPortPa()) - base_ + 1u;
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
        if (kind == ResetLineKind::Rtc) PowerOnReset();
    });
    emu_.Get<PeripheralDispatcher>().Register(this);
    PowerOnReset();
}

void Pdiusbd12::PowerOnReset() {
    command_         = 0u;
    phase_           = 0u;
    configuration_   = kPowerOnConfiguration;
    clock_division_  = kPowerOnClockDivision;
    dma_             = 0u;
    address_enable_  = 0u;
    endpoint_enable_ = 0u;
    selected_        = 0u;
    pointer_         = 0u;
    interrupt_       = 0u;
    suspended_       = 0u;
    endpoints_       = {};
    ArmSuspend();
    PublishInterrupt();
}

uint8_t Pdiusbd12::ReadByte(uint32_t addr) {
    if (addr != config_->DataPortPa()) HaltUnsupportedAccess("ReadByte", addr, 0u);
    return ReadData(addr);
}

void Pdiusbd12::WriteByte(uint32_t addr, uint8_t value) {
    if (addr == config_->CommandPortPa()) {
        Command(addr, value);
        return;
    }
    if (addr != config_->DataPortPa()) HaltUnsupportedAccess("WriteByte", addr, value);
    WriteData(addr, value);
}

void Pdiusbd12::SetSuspendHeldLow(bool held) {
    if ((held_low_ != 0u) == held) return;
    held_low_ = held ? 1u : 0u;
    if (held) {
        if (suspended_ != 0u) Wake();
        clock_->Disarm(*suspend_timer_);
        return;
    }
    if (suspended_ == 0u) ArmSuspend();
}

void Pdiusbd12::EnterSuspend() {
    suspended_ = 1u;
    SetInterrupt(kSuspendChange);
}

void Pdiusbd12::Wake() {
    suspended_ = 0u;
    SetInterrupt(kSuspendChange);
}

void Pdiusbd12::ArmSuspend() {
    if (held_low_ != 0u) return;
    clock_->Arm(*suspend_timer_, kSuspendAfterNs);
}

int64_t Pdiusbd12::ParkWakeDueNs() {
    if (interrupt_ != 0u || (clock_division_ & kSofOnlyInterrupts) != 0u) return GuestDeepSleep::kNoParkWake;
    if (!config_->InterruptWakesStandby()) return GuestDeepSleep::kNoParkWake;
    return clock_->SleptNsAtDue(*suspend_timer_);
}

void Pdiusbd12::SetInterrupt(uint16_t bits) {
    interrupt_ = static_cast<uint16_t>(interrupt_ | bits);
    PublishInterrupt();
}

void Pdiusbd12::ClearInterrupt(uint16_t bits) {
    interrupt_ = static_cast<uint16_t>(interrupt_ & ~bits);
    PublishInterrupt();
}

void Pdiusbd12::PublishInterrupt() {
    config_->DriveInterrupt((clock_division_ & kSofOnlyInterrupts) == 0u && interrupt_ != 0u);
}

uint8_t Pdiusbd12::Mode() const { return static_cast<uint8_t>(configuration_ >> kEndpointConfigShift); }

bool Pdiusbd12::Exists(size_t index) const {
    if (index == kMainOut) return Mode() != kModeIsoIn;
    if (index == kMainIn) return Mode() != kModeIsoOut;
    return index < kEndpointCount;
}

size_t Pdiusbd12::PacketBytes(size_t index) const {
    if (index != kMainOut && index != kMainIn) return kGenericPacketBytes;
    return Mode() == kModeIsoOut || Mode() == kModeIsoIn ? kIsoPacketBytes : kMainPacketBytes;
}

void Pdiusbd12::RequireEndpoint(uint32_t addr, size_t index) {
    if (Exists(index)) return;
    emu_.Get<Fatal>().Die("Pdiusbd12: command 0x%02X at 0x%08X names endpoint %zu, absent in mode %u", command_, addr,
                          index, static_cast<unsigned>(Mode()));
}

void Pdiusbd12::Command(uint32_t addr, uint8_t value) {
    command_ = value;
    phase_   = 0u;
    if (value < kEndpointCount) {
        RequireEndpoint(addr, value);
        selected_ = value;
        pointer_  = 0u;
        return;
    }
    if (value >= kEndpointStatusBase && value < kEndpointStatusBase + kEndpointCount) {
        RequireEndpoint(addr, value - kEndpointStatusBase);
        return;
    }
    switch (value) {
        case kSetAddressEnable:
        case kSetEndpointEnable:
        case kSetMode:
        case kSetDma:
        case kReadInterrupt:
        case kBuffer:
        case kReadChipId: return;
        case kClearBuffer: ClearBuffer(addr); return;
        case kValidateBuffer: ValidateBuffer(addr); return;
        default: HaltUnsupportedAccess("WriteByte(command)", addr, value);
    }
}

void Pdiusbd12::ValidateBuffer(uint32_t addr) {
    Endpoint& ep     = endpoints_[selected_];
    Buffer&   buffer = ep.buffers[ep.cpu_buffer];
    if (!IsIn(selected_) || buffer.full != 0u) {
        emu_.Get<Fatal>().Die("Pdiusbd12: Validate Buffer on endpoint %u (full %u) at 0x%08X", selected_, buffer.full,
                              addr);
    }
    buffer.full = 1u;
    if (selected_ == kMainIn) {
        ep.cpu_buffer ^= 1u;
        if ((dma_ & kMainInInterruptEnable) != 0u) SetInterrupt(EndpointInterrupt(kMainIn));
    }
}

void Pdiusbd12::ClearBuffer(uint32_t addr) {
    Endpoint& ep     = endpoints_[selected_];
    Buffer&   buffer = ep.buffers[ep.cpu_buffer];
    if (IsIn(selected_) || buffer.full == 0u) {
        emu_.Get<Fatal>().Die("Pdiusbd12: Clear Buffer on endpoint %u (full %u) at 0x%08X", selected_, buffer.full,
                              addr);
    }
    buffer.full = 0u;
    if (selected_ == kMainOut) ep.cpu_buffer ^= 1u;
}

uint8_t Pdiusbd12::ReadData(uint32_t addr) {
    const uint8_t phase = phase_++;
    if (command_ < kEndpointCount && phase == 0u) {
        const Endpoint& ep = endpoints_[command_];
        return static_cast<uint8_t>((ep.buffers[ep.cpu_buffer].full != 0u ? kSelectFull : 0u) |
                                    (ep.stalled != 0u ? kSelectStall : 0u));
    }
    if (command_ >= kEndpointStatusBase && command_ < kEndpointStatusBase + kEndpointCount && phase == 0u) {
        return ReadTransactionStatus(command_ - kEndpointStatusBase);
    }
    switch (command_) {
        case kSetDma:
            if (phase == 0u) return dma_;
            break;
        case kReadInterrupt:
            if (phase < 2u) return ReadInterrupt(phase);
            break;
        case kReadChipId:
            if (phase < kChipId.size()) return kChipId[phase];
            break;
        case kBuffer: return ReadBuffer(addr);
        default: break;
    }
    emu_.Get<Fatal>().Die("Pdiusbd12: data read %u after command 0x%02X at 0x%08X", phase, command_, addr);
}

uint8_t Pdiusbd12::ReadInterrupt(uint8_t phase) {
    const unsigned shift = phase * 8u;
    const uint8_t  value = static_cast<uint8_t>(interrupt_ >> shift);
    ClearInterrupt(static_cast<uint16_t>(kClearedOnRead & (0xFFu << shift)));
    return value;
}

uint8_t Pdiusbd12::ReadTransactionStatus(size_t index) {
    ClearInterrupt(EndpointInterrupt(index));
    return 0u;
}

void Pdiusbd12::WriteData(uint32_t addr, uint8_t value) {
    const uint8_t phase = phase_++;
    if (command_ >= kEndpointStatusBase && command_ < kEndpointStatusBase + kEndpointCount && phase == 0u) {
        SetEndpointStatus(addr, command_ - kEndpointStatusBase, value);
        return;
    }
    switch (command_) {
        case kSetAddressEnable:
            if (phase != 0u) break;
            address_enable_ = value;
            return;
        case kSetEndpointEnable:
            if (phase != 0u) break;
            if ((value & ~kGenericEndpointsEnable) != 0u) HaltUnsupportedAccess("WriteByte(endpoint enable)", addr, value);
            endpoint_enable_ = value;
            return;
        case kSetMode:
            if (phase == 0u) {
                if ((value & kConfigurationReserved) != 0u) HaltUnsupportedAccess("WriteByte(mode reserved)", addr, value);
                configuration_ = value;
                return;
            }
            if (phase != 1u) break;
            if ((value & kClockDivisionReserved) != 0u) HaltUnsupportedAccess("WriteByte(clock reserved)", addr, value);
            clock_division_ = value;
            PublishInterrupt();
            return;
        case kSetDma:
            if (phase != 0u) break;
            if ((value & kDmaEnable) != 0u) HaltUnsupportedAccess("WriteByte(DMA enable)", addr, value);
            dma_ = value;
            return;
        case kBuffer: WriteBuffer(addr, value); return;
        default: break;
    }
    emu_.Get<Fatal>().Die("Pdiusbd12: data write %u of 0x%02X after command 0x%02X at 0x%08X", phase, value, command_,
                          addr);
}

void Pdiusbd12::SetEndpointStatus(uint32_t addr, size_t index, uint8_t value) {
    if ((value & ~kStall) != 0u) HaltUnsupportedAccess("WriteByte(endpoint status)", addr, value);
    Endpoint& ep = endpoints_[index];
    ep.stalled   = value & kStall;
    if (ep.stalled == 0u) {
        for (Buffer& buffer : ep.buffers) buffer.full = 0u;
        ep.cpu_buffer = 0u;
    }
}

size_t Pdiusbd12::BufferIndex(uint32_t addr) {
    const size_t limit = kBufferHeaderBytes + PacketBytes(selected_);
    if (pointer_ >= limit) {
        emu_.Get<Fatal>().Die("Pdiusbd12: buffer access %u past the %zu-byte buffer of endpoint %u at 0x%08X", pointer_,
                              limit, selected_, addr);
    }
    return pointer_++;
}

uint8_t Pdiusbd12::ReadBuffer(uint32_t addr) {
    const Endpoint& ep = endpoints_[selected_];
    if (IsIn(selected_) || ep.buffers[ep.cpu_buffer].full == 0u) {
        emu_.Get<Fatal>().Die("Pdiusbd12: Read Buffer on endpoint %u (full %u) at 0x%08X", selected_,
                              ep.buffers[ep.cpu_buffer].full, addr);
    }
    return endpoints_[selected_].buffers[ep.cpu_buffer].bytes[BufferIndex(addr)];
}

void Pdiusbd12::WriteBuffer(uint32_t addr, uint8_t value) {
    Endpoint& ep = endpoints_[selected_];
    if (!IsIn(selected_) || ep.buffers[ep.cpu_buffer].full != 0u) {
        emu_.Get<Fatal>().Die("Pdiusbd12: Write Buffer on endpoint %u (full %u) at 0x%08X", selected_,
                              ep.buffers[ep.cpu_buffer].full, addr);
    }
    ep.buffers[ep.cpu_buffer].bytes[BufferIndex(addr)] = value;
}

void Pdiusbd12::SaveState(StateWriter& w) {
    static_assert(StateVisitCoversAllBytes<Endpoint>([](Endpoint& e, StateFieldBytes& f) { VisitEndpoint(e, f); }),
                  "Pdiusbd12::VisitEndpoint must name or skip every field of an endpoint");
    w.Write("command", command_);
    w.Write("data_phase", phase_);
    w.Write("configuration", configuration_);
    w.Write("clock_division", clock_division_);
    w.Write("dma_configuration", dma_);
    w.Write("address_enable", address_enable_);
    w.Write("endpoint_enable", endpoint_enable_);
    w.Write("selected_endpoint", selected_);
    w.Write("buffer_pointer", pointer_);
    w.Write("interrupt", interrupt_);
    w.Write("suspended", suspended_);
    w.Write("suspend_held_low", held_low_);
    clock_->SaveTimer(*suspend_timer_, w, "suspend_");
    StateWriteField field(w);
    for (uint32_t i = 0; i < kEndpointCount; ++i) {
        w.BeginFrame(i);
        VisitEndpoint(endpoints_[i], field);
        w.EndFrame();
    }
}

void Pdiusbd12::RestoreState(StateReader& r) {
    r.Read("command", command_);
    r.Read("data_phase", phase_);
    r.Read("configuration", configuration_);
    r.Read("clock_division", clock_division_);
    r.Read("dma_configuration", dma_);
    r.Read("address_enable", address_enable_);
    r.Read("endpoint_enable", endpoint_enable_);
    r.Read("selected_endpoint", selected_);
    r.Read("buffer_pointer", pointer_);
    r.Read("interrupt", interrupt_);
    r.Read("suspended", suspended_);
    r.Read("suspend_held_low", held_low_);
    clock_->RestoreTimer(*suspend_timer_, r, "suspend_");
    StateReadField field(r);
    for (Endpoint& ep : endpoints_) {
        r.EnterFrame();
        VisitEndpoint(ep, field);
        r.LeaveFrame();
    }
}

void Pdiusbd12::PostRestore() {
    clock_->ResumeTimer(*suspend_timer_);
    PublishInterrupt();
}

}

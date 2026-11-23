#include "intel_21152_bridge.h"

#include "../../core/board_parts.h"
#include "intel_21152_part_id.h"
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../pci/pci_host_bridge.h"
#include "intel_21152_board.h"

#include <utility>

namespace cerfcore {

namespace {

constexpr PciCommonIdentity  kIdentity{0xB1528086u, 0x06040000u, 0x0290u, 0x0000u, 0x0367u, 0xFFu, 0xFFu, 0x01u};
constexpr PciPowerManagement kPowerManagement{0xDCu, 0x00010001u};

constexpr uint16_t kCommandMaster   = 0x0004u;
constexpr uint16_t kCommandVgaSnoop = 0x0020u;

constexpr uint32_t kRegBusNumbers         = 0x18u;
constexpr uint32_t kRegIoBaseLimit        = 0x1Cu;
constexpr uint32_t kRegMemory             = 0x20u;
constexpr uint32_t kRegPrefetch           = 0x24u;
constexpr uint32_t kRegPrefetchBaseUpper  = 0x28u;
constexpr uint32_t kRegPrefetchLimitUpper = 0x2Cu;
constexpr uint32_t kRegIoUpper            = 0x30u;
constexpr uint32_t kRegCapPtr             = 0x34u;
constexpr uint32_t kRegBridgeControl      = 0x3Cu;
constexpr uint32_t kRegDeviceControl      = 0x40u;
constexpr uint32_t kRegSerrEvent          = 0x64u;
constexpr uint32_t kRegClockControl       = 0x68u;

constexpr uint16_t kSecondaryStatus             = 0x0280u;
constexpr uint16_t kSecondaryStatusW1tc         = 0xF900u;
constexpr uint16_t kSecondaryReceivedMasterAbort = 0x2000u;
constexpr uint8_t  kIoAddressing32   = 0x01u;
constexpr uint8_t  kIoWindowMask     = 0xF0u;
constexpr uint16_t kMemoryWindowMask = 0xFFF0u;
constexpr uint16_t kPrefetch64       = 0x0001u;
constexpr uint32_t kIoGranuleLow     = 0x00000FFFu;
constexpr uint32_t kMemoryGranuleLow = 0x000FFFFFu;

constexpr uint16_t kBcIsaEnable       = 0x0004u;
constexpr uint16_t kBcVgaEnable       = 0x0008u;
constexpr uint16_t kBcMasterAbortMode = 0x0020u;
constexpr uint16_t kBcSecondaryReset  = 0x0040u;
constexpr uint16_t kBcWritable        = 0x0BEFu;

constexpr uint8_t  kChipControlWritable = 0x12u;
constexpr uint8_t  kDiagChipReset       = 0x01u;
constexpr uint8_t  kDiagTestMode        = 0x06u;
constexpr uint16_t kArbiterWritable     = 0x03FFu;
constexpr uint16_t kArbiterReset        = 0x0200u;
constexpr uint8_t  kSerrEventWritable   = 0x7Eu;
constexpr uint16_t kClockControlBits    = 0x01FFu;
constexpr uint16_t kClockReservedOnes   = 0x3E00u;
constexpr uint16_t kClockPairMask       = 0x3u;
constexpr uint16_t kClock4Disable       = 0x0100u;
constexpr uint32_t kClockPairs          = 4u;

constexpr uint32_t kIsaFirst64k  = 0x00010000u;
constexpr uint32_t kIsaAliasMask = 0x00000300u;
constexpr uint32_t kVgaMemFirst  = 0x000A0000u;
constexpr uint32_t kVgaMemLast   = 0x000BFFFFu;
constexpr uint32_t kVgaIoMask    = 0x000003FFu;

constexpr uint32_t kType1BusShift      = 16u;
constexpr uint32_t kType1DevShift      = 11u;
constexpr uint32_t kType1FnShift       = 8u;
constexpr uint32_t kType1FieldMask     = 0xFFu;
constexpr uint32_t kType1DevMask       = 0x1Fu;
constexpr uint32_t kType1FnMask        = 0x7u;
constexpr uint32_t kType1RegMask       = 0xFCu;
constexpr uint32_t kIdselDevices       = 16u;
constexpr uint32_t kSpecialCycleDevice = 0x1Fu;
constexpr uint32_t kSpecialCycleFn     = 0x7u;
constexpr uint32_t kAllOnes            = 0xFFFFFFFFu;

bool ReservedRegister(uint32_t reg) {
    return reg == 0x10u || reg == 0x14u || reg == 0x38u || (reg >= 0x44u && reg <= 0x60u) ||
           (reg >= 0x6Cu && reg <= 0xD8u) || reg >= 0xE4u;
}

bool ClockDisabled(uint32_t value) {
    for (uint32_t pair = 0; pair < kClockPairs; ++pair)
        if (((value >> (2u * pair)) & kClockPairMask) == kClockPairMask) return true;
    return (value & kClock4Disable) != 0u;
}

}

REGISTER_SERVICE(Intel21152Bridge);

Intel21152Bridge::Intel21152Bridge(CerfEmulator& emu)
    : Service(emu), header_(kIdentity), pm_(kPowerManagement), secondary_("21152 secondary") {}

bool Intel21152Bridge::ShouldRegister() {
    return emu_.Get<BoardParts>().Has(PartId::Intel21152);
}

void Intel21152Bridge::OnReady() {
    primary_ = &emu_.Get<PciHostBridge>();
    dev_     = primary_->DeviceOfIdselLine(emu_.Get<Intel21152Board>().BridgeIdselLine());
    ResetRegisters();
    primary_->RegisterPciDevice(this);
    primary_->RegisterBusResetListener([this] { ResetChip(false); });
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { ResetChip(false); });
}

void Intel21152Bridge::Reject(const char* what, uint32_t addr, uint32_t value) const {
    emu_.Get<Fatal>().Die("Intel 21152 bridge: %s addr=0x%08X val=0x%08X command=0x%04X bridge_control=0x%04X "
                          "buses=%u/%u/%u",
                          what, addr, value, header_.Command(), bridge_control_, primary_bus_, secondary_bus_,
                          subordinate_bus_);
}

void Intel21152Bridge::ResetRegisters() {
    header_.Reset();
    pm_                 = kPowerManagement;
    primary_bus_        = 0u;
    secondary_bus_      = 0u;
    subordinate_bus_    = 0u;
    secondary_latency_  = 0u;
    io_base_            = 0u;
    io_limit_           = 0u;
    io_base_upper_      = 0u;
    io_limit_upper_     = 0u;
    mem_base_           = 0u;
    mem_limit_          = 0u;
    pref_base_          = 0u;
    pref_limit_         = 0u;
    pref_base_upper_    = 0u;
    pref_limit_upper_   = 0u;
    bridge_control_     = 0u;
    chip_control_       = 0u;
    arbiter_control_    = kArbiterReset;
    serr_event_disable_ = 0u;
    clock_control_      = 0u;
    secondary_status_   = 0u;
}

void Intel21152Bridge::ResetChip(bool hold_secondary_reset) {
    ResetRegisters();
    if (hold_secondary_reset) bridge_control_ |= kBcSecondaryReset;
    secondary_reset_.Call();
}

uint32_t Intel21152Bridge::ConfigRead(uint32_t reg) {
    uint32_t value = 0u;
    if (header_.Read(reg, &value)) return value;
    if (pm_.Owns(reg)) return pm_.Read(reg);
    if (ReservedRegister(reg)) return 0u;
    switch (reg) {
        case kRegBusNumbers:
            return primary_bus_ | (static_cast<uint32_t>(secondary_bus_) << 8) |
                   (static_cast<uint32_t>(subordinate_bus_) << 16) | (static_cast<uint32_t>(secondary_latency_) << 24);
        case kRegIoBaseLimit:
            return (io_base_ | kIoAddressing32) | (static_cast<uint32_t>(io_limit_ | kIoAddressing32) << 8) |
                   (static_cast<uint32_t>(kSecondaryStatus | secondary_status_) << 16);
        case kRegMemory:             return mem_base_ | (static_cast<uint32_t>(mem_limit_) << 16);
        case kRegPrefetch:           return (pref_base_ | kPrefetch64) | (static_cast<uint32_t>(pref_limit_ | kPrefetch64) << 16);
        case kRegPrefetchBaseUpper:  return pref_base_upper_;
        case kRegPrefetchLimitUpper: return pref_limit_upper_;
        case kRegIoUpper:            return io_base_upper_ | (static_cast<uint32_t>(io_limit_upper_) << 16);
        case kRegCapPtr:             return pm_.offset;
        case kRegBridgeControl:      return static_cast<uint32_t>(bridge_control_) << 16;
        case kRegDeviceControl:      return chip_control_ | (static_cast<uint32_t>(arbiter_control_) << 16);
        case kRegSerrEvent:          return serr_event_disable_;
        case kRegClockControl:       return clock_control_ | kClockReservedOnes;
        default:                     Reject("configuration read of a register whose read value is not modeled", reg, 0u);
    }
}

void Intel21152Bridge::ConfigWrite(uint32_t reg, uint32_t value) {
    if (reg == PciCommonHeader::kRegCommand && (value & kCommandVgaSnoop) != 0u) Reject("VGA snoop enable", reg, value);
    if (header_.Write(reg, value)) return;
    if (pm_.Owns(reg)) {
        if (const char* why = pm_.WriteHoldingD0(reg, value)) Reject(why, reg, value);
        return;
    }
    if (ReservedRegister(reg)) return;
    switch (reg) {
        case PciCommonHeader::kRegIdentity:
        case PciCommonHeader::kRegClass:
        case kRegCapPtr:
            return;
        case kRegBusNumbers:
            primary_bus_       = static_cast<uint8_t>(value);
            secondary_bus_     = static_cast<uint8_t>(value >> 8);
            subordinate_bus_   = static_cast<uint8_t>(value >> 16);
            secondary_latency_ = static_cast<uint8_t>(value >> 24);
            return;
        case kRegIoBaseLimit:
            io_base_  = static_cast<uint8_t>(value & kIoWindowMask);
            io_limit_ = static_cast<uint8_t>((value >> 8) & kIoWindowMask);
            secondary_status_ &= static_cast<uint16_t>(~((value >> 16) & kSecondaryStatusW1tc));
            return;
        case kRegMemory:
            mem_base_  = static_cast<uint16_t>(value & kMemoryWindowMask);
            mem_limit_ = static_cast<uint16_t>((value >> 16) & kMemoryWindowMask);
            return;
        case kRegPrefetch:
            pref_base_  = static_cast<uint16_t>(value & kMemoryWindowMask);
            pref_limit_ = static_cast<uint16_t>((value >> 16) & kMemoryWindowMask);
            return;
        case kRegPrefetchBaseUpper:  pref_base_upper_ = value; return;
        case kRegPrefetchLimitUpper: pref_limit_upper_ = value; return;
        case kRegIoUpper:
            io_base_upper_  = static_cast<uint16_t>(value);
            io_limit_upper_ = static_cast<uint16_t>(value >> 16);
            return;
        case kRegBridgeControl: WriteBridgeControl(static_cast<uint16_t>(value >> 16)); return;
        case kRegDeviceControl: WriteDeviceControl(value); return;
        case kRegSerrEvent:     serr_event_disable_ = static_cast<uint8_t>(value & kSerrEventWritable); return;
        case kRegClockControl:
            if (ClockDisabled(value)) Reject("secondary clock disable", reg, value);
            clock_control_ = static_cast<uint16_t>(value & kClockControlBits);
            return;
        default:
            Reject("configuration write of a register whose effect is not modeled", reg, value);
    }
}

void Intel21152Bridge::WriteBridgeControl(uint16_t value) {
    const bool was_reset = (bridge_control_ & kBcSecondaryReset) != 0u;
    bridge_control_      = static_cast<uint16_t>(value & kBcWritable);
    if ((bridge_control_ & kBcSecondaryReset) != 0u && !was_reset) {
        clock_control_ = 0u;
        secondary_reset_.Call();
    }
}

void Intel21152Bridge::WriteDeviceControl(uint32_t value) {
    const uint8_t diag = static_cast<uint8_t>(value >> 8);
    if ((diag & kDiagTestMode) != 0u) Reject("counter test mode", kRegDeviceControl, value);
    chip_control_    = static_cast<uint8_t>(value & kChipControlWritable);
    arbiter_control_ = static_cast<uint16_t>((value >> 16) & kArbiterWritable);
    if ((diag & kDiagChipReset) != 0u) ResetChip(true);
}

bool Intel21152Bridge::InIoWindow(uint32_t addr) const {
    const uint32_t base  = (static_cast<uint32_t>(io_base_upper_) << 16) | (static_cast<uint32_t>(io_base_) << 8);
    const uint32_t limit = (static_cast<uint32_t>(io_limit_upper_) << 16) | (static_cast<uint32_t>(io_limit_) << 8) |
                           kIoGranuleLow;
    if (addr < base || addr > limit) return false;
    return (bridge_control_ & kBcIsaEnable) == 0u || addr >= kIsaFirst64k || (addr & kIsaAliasMask) == 0u;
}

bool Intel21152Bridge::InMemoryWindows(uint32_t addr) const {
    const uint32_t base  = static_cast<uint32_t>(mem_base_) << 16;
    const uint32_t limit = (static_cast<uint32_t>(mem_limit_) << 16) | kMemoryGranuleLow;
    if (addr >= base && addr <= limit) return true;
    const uint64_t pref_base  = (static_cast<uint64_t>(pref_base_upper_) << 32) | (static_cast<uint64_t>(pref_base_) << 16);
    const uint64_t pref_limit = (static_cast<uint64_t>(pref_limit_upper_) << 32) |
                                (static_cast<uint64_t>(pref_limit_) << 16) | kMemoryGranuleLow;
    return addr >= pref_base && addr <= pref_limit;
}

bool Intel21152Bridge::VgaIo(uint32_t addr) const {
    if ((bridge_control_ & kBcVgaEnable) == 0u || addr >= kIsaFirst64k) return false;
    const uint32_t a = addr & kVgaIoMask;
    return (a >= 0x3B0u && a <= 0x3BBu) || (a >= 0x3C0u && a <= 0x3DFu);
}

bool Intel21152Bridge::VgaMemory(uint32_t addr) const {
    return (bridge_control_ & kBcVgaEnable) != 0u && addr >= kVgaMemFirst && addr <= kVgaMemLast;
}

bool Intel21152Bridge::MemClaims(uint32_t pci_addr) const {
    return (header_.Command() & PciCommonHeader::kCommandMemory) != 0u &&
           (InMemoryWindows(pci_addr) || VgaMemory(pci_addr));
}

bool Intel21152Bridge::IoClaims(uint32_t pci_io) const {
    return (header_.Command() & PciCommonHeader::kCommandIo) != 0u && (InIoWindow(pci_io) || VgaIo(pci_io));
}

void Intel21152Bridge::RequireSecondaryOutOfReset(const char* what, uint32_t addr) const {
    if ((bridge_control_ & kBcSecondaryReset) != 0u) Reject(what, addr, 0u);
}

PciDevice* Intel21152Bridge::SecondaryClaimer(bool (PciDevice::*claims)(uint32_t) const, uint32_t addr,
                                              const char* what) const {
    RequireSecondaryOutOfReset(what, addr);
    return secondary_.Claimer(claims, addr);
}

void Intel21152Bridge::SecondaryMasterAbort(const char* what, uint32_t addr, uint32_t value) {
    secondary_status_ |= kSecondaryReceivedMasterAbort;
    if ((bridge_control_ & kBcMasterAbortMode) != 0u) Reject(what, addr, value);
}

uint32_t Intel21152Bridge::MasterAbortRead(const char* what, uint32_t addr) {
    SecondaryMasterAbort(what, addr, 0u);
    return kAllOnes;
}

uint32_t Intel21152Bridge::MemRead(uint32_t pci_addr, unsigned size) {
    PciDevice* d = SecondaryClaimer(&PciDevice::MemClaims, pci_addr, "memory read while the secondary bus is in reset");
    return d ? d->MemRead(pci_addr, size)
             : MasterAbortRead("master abort of a forwarded memory read in target-abort mode", pci_addr) &
                   cerf::ByteWidthMask(size);
}

void Intel21152Bridge::MemWrite(uint32_t pci_addr, uint32_t value, unsigned size) {
    PciDevice* d = SecondaryClaimer(&PciDevice::MemClaims, pci_addr, "memory write while the secondary bus is in reset");
    if (d) {
        d->MemWrite(pci_addr, value, size);
        return;
    }
    SecondaryMasterAbort("master abort of a forwarded memory write in target-abort mode", pci_addr, value);
}

uint32_t Intel21152Bridge::IoRead(uint32_t pci_io, unsigned size) {
    PciDevice* d = SecondaryClaimer(&PciDevice::IoClaims, pci_io, "I/O read while the secondary bus is in reset");
    return d ? d->IoRead(pci_io, size)
             : MasterAbortRead("master abort of a forwarded I/O read in target-abort mode", pci_io) &
                   cerf::ByteWidthMask(size);
}

void Intel21152Bridge::IoWrite(uint32_t pci_io, uint32_t value, unsigned size) {
    PciDevice* d = SecondaryClaimer(&PciDevice::IoClaims, pci_io, "I/O write while the secondary bus is in reset");
    if (d) {
        d->IoWrite(pci_io, value, size);
        return;
    }
    SecondaryMasterAbort("master abort of a forwarded I/O write in target-abort mode", pci_io, value);
}

bool Intel21152Bridge::Type1ConfigClaims(uint32_t bus) const {
    return bus == secondary_bus_ || (bus > secondary_bus_ && bus <= subordinate_bus_);
}

uint32_t Intel21152Bridge::SecondaryConfigRegister(uint32_t address) const { return address & kType1RegMask; }

PciDevice* Intel21152Bridge::SecondaryConfigTarget(uint32_t address, const char* what) const {
    RequireSecondaryOutOfReset(what, address);
    const uint32_t dev = (address >> kType1DevShift) & kType1DevMask;
    const uint32_t fn  = (address >> kType1FnShift) & kType1FnMask;
    if (dev == kSpecialCycleDevice && fn == kSpecialCycleFn && SecondaryConfigRegister(address) == 0u)
        Reject("special cycle generation on the secondary bus", address, 0u);
    if (dev >= kIdselDevices) return nullptr;
    return secondary_.ConfigDevice(static_cast<uint8_t>(dev), fn);
}

uint32_t Intel21152Bridge::Type1ConfigRead(uint32_t address) {
    const uint32_t bus = (address >> kType1BusShift) & kType1FieldMask;
    PciDevice*     d   = bus == secondary_bus_
                             ? SecondaryConfigTarget(address, "configuration read while the secondary bus is in reset")
                             : SecondaryClaimer(&PciDevice::Type1ConfigClaims, bus,
                                                "configuration read while the secondary bus is in reset");
    if (!d) return MasterAbortRead("master abort of a forwarded configuration read in target-abort mode", address);
    return bus == secondary_bus_ ? d->ConfigRead(SecondaryConfigRegister(address)) : d->Type1ConfigRead(address);
}

void Intel21152Bridge::Type1ConfigWrite(uint32_t address, uint32_t value) {
    const uint32_t bus = (address >> kType1BusShift) & kType1FieldMask;
    PciDevice*     d   = bus == secondary_bus_
                             ? SecondaryConfigTarget(address, "configuration write while the secondary bus is in reset")
                             : SecondaryClaimer(&PciDevice::Type1ConfigClaims, bus,
                                                "configuration write while the secondary bus is in reset");
    if (!d) {
        SecondaryMasterAbort("master abort of a forwarded configuration write in target-abort mode", address, value);
        return;
    }
    if (bus == secondary_bus_) d->ConfigWrite(SecondaryConfigRegister(address), value);
    else                       d->Type1ConfigWrite(address, value);
}

void Intel21152Bridge::RegisterPciDevice(PciDevice* dev) { secondary_.RegisterDevice(dev); }

void Intel21152Bridge::RegisterBusResetListener(std::function<void()> listener) {
    secondary_reset_.Add(std::move(listener));
}

uint32_t Intel21152Bridge::BusMasterRead32(uint32_t pci_addr) {
    if ((header_.Command() & kCommandMaster) == 0u) Reject("secondary bus master read with master enable clear", pci_addr, 0u);
    if (InMemoryWindows(pci_addr) || VgaMemory(pci_addr)) Reject("peer-to-peer secondary bus master read", pci_addr, 0u);
    return primary_->BusMasterRead32(pci_addr);
}

void Intel21152Bridge::BusMasterWrite32(uint32_t pci_addr, uint32_t value) {
    if ((header_.Command() & kCommandMaster) == 0u)
        Reject("secondary bus master write with master enable clear", pci_addr, value);
    if (InMemoryWindows(pci_addr) || VgaMemory(pci_addr)) Reject("peer-to-peer secondary bus master write", pci_addr, value);
    primary_->BusMasterWrite32(pci_addr, value);
}

void Intel21152Bridge::SaveState(StateWriter& w) {
    StateWriteField field(w);
    PciCommonHeader::Visit(header_, field);
    PciPowerManagement::Visit(pm_, field);
    w.Write("b152_primary_bus", primary_bus_);
    w.Write("b152_secondary_bus", secondary_bus_);
    w.Write("b152_subordinate_bus", subordinate_bus_);
    w.Write("b152_secondary_latency", secondary_latency_);
    w.Write("b152_io_base", io_base_);
    w.Write("b152_io_limit", io_limit_);
    w.Write("b152_io_base_upper", io_base_upper_);
    w.Write("b152_io_limit_upper", io_limit_upper_);
    w.Write("b152_mem_base", mem_base_);
    w.Write("b152_mem_limit", mem_limit_);
    w.Write("b152_pref_base", pref_base_);
    w.Write("b152_pref_limit", pref_limit_);
    w.Write("b152_pref_base_upper", pref_base_upper_);
    w.Write("b152_pref_limit_upper", pref_limit_upper_);
    w.Write("b152_bridge_control", bridge_control_);
    w.Write("b152_chip_control", chip_control_);
    w.Write("b152_arbiter_control", arbiter_control_);
    w.Write("b152_serr_event_disable", serr_event_disable_);
    w.Write("b152_clock_control", clock_control_);
    w.Write("b152_secondary_status", secondary_status_);
    secondary_.SaveState(w);
}

void Intel21152Bridge::RestoreState(StateReader& r) {
    StateReadField field(r);
    PciCommonHeader::Visit(header_, field);
    PciPowerManagement::Visit(pm_, field);
    r.Read("b152_primary_bus", primary_bus_);
    r.Read("b152_secondary_bus", secondary_bus_);
    r.Read("b152_subordinate_bus", subordinate_bus_);
    r.Read("b152_secondary_latency", secondary_latency_);
    r.Read("b152_io_base", io_base_);
    r.Read("b152_io_limit", io_limit_);
    r.Read("b152_io_base_upper", io_base_upper_);
    r.Read("b152_io_limit_upper", io_limit_upper_);
    r.Read("b152_mem_base", mem_base_);
    r.Read("b152_mem_limit", mem_limit_);
    r.Read("b152_pref_base", pref_base_);
    r.Read("b152_pref_limit", pref_limit_);
    r.Read("b152_pref_base_upper", pref_base_upper_);
    r.Read("b152_pref_limit_upper", pref_limit_upper_);
    r.Read("b152_bridge_control", bridge_control_);
    r.Read("b152_chip_control", chip_control_);
    r.Read("b152_arbiter_control", arbiter_control_);
    r.Read("b152_serr_event_disable", serr_event_disable_);
    r.Read("b152_clock_control", clock_control_);
    r.Read("b152_secondary_status", secondary_status_);
    secondary_.RestoreState(r);
}

void Intel21152Bridge::PostRestore() { secondary_.PostRestore(); }

}

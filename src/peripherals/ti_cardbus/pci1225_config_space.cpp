#include "pci1225_config_space.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kVendorDevice      = 0xAC1C104Cu;
constexpr uint32_t kClassRevision     = 0x06070001u;
constexpr uint32_t kHeaderTypeBist    = 0x00820000u;
constexpr uint32_t kStatus            = 0x0210u;
constexpr uint32_t kCapabilityPointer = 0xA0u;
constexpr uint32_t kSecondaryStatus   = 0x0200u;

constexpr uint16_t kCommandWritable   = 0x0147u;
constexpr uint32_t kSocketBarWritable = 0xFFFFF000u;
constexpr uint32_t kMemWindowWritable = 0xFFFFF000u;
constexpr uint32_t kIoBaseWritable    = 0xFFFFFFFCu;
constexpr uint32_t kIoLimitWritable   = 0x0000FFFCu;

constexpr uint16_t kBridgeControlWritable = 0x07EEu;
constexpr uint16_t kBridgeGlobal          = 0x0023u;
constexpr uint16_t kBridgeControlReset    = 0x0340u;
constexpr uint8_t  kInterruptLineReset    = 0xFFu;

constexpr uint32_t kLegacyBaseReadOne = 0x00000001u;

constexpr uint32_t kReservedFirstA       = 0x48u;
constexpr uint32_t kReservedLastA        = 0x7Cu;
constexpr uint32_t kReserved84           = 0x84u;
constexpr uint32_t kReserved88           = 0x88u;
constexpr uint32_t kReserved9C           = 0x9Cu;
constexpr uint32_t kReservedFirstB       = 0xB4u;
constexpr uint32_t kReservedLastB        = 0xFCu;
constexpr uint32_t kReservedRegisterRead = 0x00000000u;

bool ReservedRegister(uint32_t reg) {
    return (reg >= kReservedFirstA && reg <= kReservedLastA) || reg == kReserved84 || reg == kReserved88 ||
           reg == kReserved9C || (reg >= kReservedFirstB && reg <= kReservedLastB);
}

constexpr uint32_t kSystemControlReset = 0x00449060u;
constexpr uint32_t kSysGlobalWritable  = 0xED00C03Au;
constexpr uint32_t kSysSocketWritable  = 0x027F0041u;
constexpr uint32_t kSysReadOne         = 0x00001000u;
constexpr uint32_t kSysSocketActivity  = 0x00002000u;
constexpr uint32_t kSysSmiStatus       = 0x02000000u;
constexpr uint32_t kSysSmiEnable       = 0x01000000u;
constexpr uint32_t kSysSmiRoute        = 0x04000000u;
constexpr uint32_t kSysVccProtectOff   = 0x00200000u;
constexpr uint32_t kSysReduceZv        = 0x00100000u;
constexpr uint32_t kSysPcPciDreq       = 0x00080000u;
constexpr uint32_t kSysPcPciDma        = 0x00000008u;
constexpr uint32_t kSysSubsysReadOnly  = 0x00000020u;

constexpr uint32_t kMfuncWritable = 0x0FFFFFFFu;

constexpr uint8_t kRetryWritable         = 0xC0u;
constexpr uint8_t kRetryReset            = 0xC0u;
constexpr uint8_t kCardControlWritable   = 0xE6u;
constexpr uint8_t kCcZoomVideo           = 0x40u;
constexpr uint8_t kCcRingIndicateEnable  = 0x80u;
constexpr uint8_t kDeviceControlWritable = 0x6Fu;
constexpr uint8_t kDeviceControlReset    = 0x66u;
constexpr uint8_t kDc3vCapable           = 0x40u;
constexpr uint8_t kDcTestBits            = 0x09u;
constexpr uint8_t kDiagnosticReset       = 0x61u;
constexpr uint8_t kDiagGlobal            = 0x9Eu;
constexpr uint8_t kDiagTrueValue         = 0x80u;

constexpr uint32_t kSocketDma0Writable = 0x00000003u;
constexpr uint32_t kSocketDma1Writable = 0x0000FFF7u;
constexpr uint32_t kSocketDmaDecode    = 0x00000001u;

constexpr uint32_t kPowerManagementCaps = 0x7E210001u;
constexpr uint32_t kPmcsrBridgeSupport  = 0x00800000u;
constexpr uint32_t kPmcsrWritable       = 0x00000103u;
constexpr uint32_t kPmcsrPowerState     = 0x00000003u;
constexpr uint32_t kPmcsrPmeEnable      = 0x00000100u;
constexpr uint32_t kPmcsrD0             = 0x00000000u;
constexpr uint32_t kPmcsrD3Hot          = 0x00000003u;

constexpr uint16_t kGpePowerChange = 0x0800u;
constexpr uint16_t kGpeVpp12Change = 0x0100u;
constexpr uint8_t  kGpoWritable    = 0x1Fu;
constexpr uint8_t  kGpiDataMask    = 0x1Fu;

constexpr uint32_t kSerialDataIndexSlave = 0x00FFFFFFu;
constexpr uint32_t kSerialControlWritable = 0x84000000u;
constexpr uint32_t kSerialControlClear    = 0x0B000000u;
constexpr uint32_t kSerialDetect          = 0x08000000u;

constexpr uint32_t WindowIndex(uint32_t reg, uint32_t first) { return (reg - first) >> 3; }

}

void Pci1225SharedConfig::Reset(uint32_t mfunc_at_power_on, bool serial_eeprom_present, uint8_t gpi_input_levels) {
    mfunc               = mfunc_at_power_on & kMfuncWritable;
    retry_status        = kRetryReset;
    device_control      = kDeviceControlReset;
    diagnostic_global   = static_cast<uint8_t>(kDiagnosticReset & kDiagGlobal);
    bridge_global       = static_cast<uint16_t>(kBridgeControlReset & kBridgeGlobal);
    system_global       = kSystemControlReset & kSysGlobalWritable;
    subsystem           = 0u;
    legacy_base         = 0u;
    legacy_index        = 0u;
    serial_bus          = serial_eeprom_present ? kSerialDetect : 0u;
    exca_global_control = 0u;
    gpi_levels          = gpi_input_levels;
}

uint32_t Pci1225SharedConfig::LegacyBase() const {
    return legacy_base & ~kLegacyBaseReadOne;
}

bool Pci1225SharedConfig::SerialBusDetected() const {
    return (serial_bus & kSerialDetect) != 0u;
}

void Pci1225SharedConfig::SaveState(StateWriter& w) const {
    w.Write("cfg_mfunc", mfunc);
    w.Write("cfg_retry_status", retry_status);
    w.Write("cfg_device_control", device_control);
    w.Write("cfg_diagnostic_global", diagnostic_global);
    w.Write("cfg_bridge_global", bridge_global);
    w.Write("cfg_system_global", system_global);
    w.Write("cfg_subsystem", subsystem);
    w.Write("cfg_legacy_base", legacy_base);
    w.Write("cfg_legacy_index", legacy_index);
    w.Write("cfg_serial_bus", serial_bus);
    w.Write("exca_global_control", exca_global_control);
}

void Pci1225SharedConfig::RestoreState(StateReader& r) {
    r.Read("cfg_mfunc", mfunc);
    r.Read("cfg_retry_status", retry_status);
    r.Read("cfg_device_control", device_control);
    r.Read("cfg_diagnostic_global", diagnostic_global);
    r.Read("cfg_bridge_global", bridge_global);
    r.Read("cfg_system_global", system_global);
    r.Read("cfg_subsystem", subsystem);
    r.Read("cfg_legacy_base", legacy_base);
    r.Read("cfg_legacy_index", legacy_index);
    r.Read("cfg_serial_bus", serial_bus);
    r.Read("exca_global_control", exca_global_control);
}

void Pci1225ConfigSpace::Reset() {
    command_     = 0u;
    cache_line_  = 0u;
    latency_     = 0u;
    socket_bar_  = 0u;
    bus_numbers_ = 0u;
    for (int i = 0; i < 2; ++i) {
        mem_base_[i]  = 0u;
        mem_limit_[i] = 0u;
        io_base_[i]   = 0u;
        io_limit_[i]  = 0u;
        socket_dma_[i] = 0u;
    }
    int_line_       = kInterruptLineReset;
    bridge_control_ = static_cast<uint16_t>(kBridgeControlReset & ~kBridgeGlobal);
    system_socket_  = kSystemControlReset & kSysSocketWritable;
    card_control_   = 0u;
    diagnostic_     = static_cast<uint8_t>(kDiagnosticReset & ~kDiagGlobal);
    pmcsr_          = 0u;
    gpe_enable_     = 0u;
    gpe_status_     = 0u;
    gpo_            = 0u;
    socket_active_  = false;
}

bool Pci1225ConfigSpace::PoweredDown() const {
    return (pmcsr_ & kPmcsrPowerState) != 0u;
}


bool Pci1225ConfigSpace::SmiPending() const {
    return (system_socket_ & kSysSmiStatus) != 0u;
}

bool Pci1225ConfigSpace::SmiToCsc() const {
    return (shared_.system_global & kSysSmiRoute) != 0u;
}

bool Pci1225ConfigSpace::ReducedZoomVideo() const {
    return (system_socket_ & kSysReduceZv) != 0u;
}

bool Pci1225ConfigSpace::IdRegistersWritable() const {
    return (shared_.system_global & kSysSubsysReadOnly) == 0u;
}

void Pci1225ConfigSpace::NotePowerRequestChange(bool vpp12_crossed) {
    gpe_status_ = static_cast<uint16_t>(gpe_status_ | kGpePowerChange | (vpp12_crossed ? kGpeVpp12Change : 0u));
    if ((shared_.system_global & kSysSmiEnable) != 0u)
        system_socket_ |= kSysSmiStatus;
}

bool Pci1225ConfigSpace::Read(uint32_t reg, uint32_t* value) {
    const uint8_t diagnostic = static_cast<uint8_t>(diagnostic_ | shared_.diagnostic_global);
    if (ReservedRegister(reg)) {
        *value = kReservedRegisterRead;
        return true;
    }
    switch (reg) {
        case 0x00:
            *value = (diagnostic & kDiagTrueValue) != 0u ? 0xFFFFFFFFu : kVendorDevice;
            return true;
        case 0x04:
            *value = (kStatus << 16) | command_;
            return true;
        case 0x08:
            *value = kClassRevision;
            return true;
        case 0x0C:
            *value = kHeaderTypeBist | (static_cast<uint32_t>(latency_) << 8) | cache_line_;
            return true;
        case 0x10:
            *value = socket_bar_;
            return true;
        case 0x14:
            *value = (kSecondaryStatus << 16) | kCapabilityPointer;
            return true;
        case 0x18:
            *value = bus_numbers_;
            return true;
        case 0x1C:
        case 0x24:
            *value = mem_base_[WindowIndex(reg, 0x1Cu)];
            return true;
        case 0x20:
        case 0x28:
            *value = mem_limit_[WindowIndex(reg, 0x20u)];
            return true;
        case 0x2C:
        case 0x34:
            *value = io_base_[WindowIndex(reg, 0x2Cu)];
            return true;
        case 0x30:
        case 0x38:
            *value = io_limit_[WindowIndex(reg, 0x30u)];
            return true;
        case 0x3C: {
            const uint16_t bridge = static_cast<uint16_t>(bridge_control_ | shared_.bridge_global);
            *value = (static_cast<uint32_t>(bridge) << 16) | (static_cast<uint32_t>(terminals_.InterruptPin()) << 8) | int_line_;
            return true;
        }
        case 0x40:
            *value = shared_.subsystem;
            return true;
        case 0x44:
            *value = shared_.legacy_base | kLegacyBaseReadOne;
            return true;
        case 0x80:
            *value = shared_.system_global | system_socket_ | kSysReadOne | (socket_active_ ? kSysSocketActivity : 0u);
            socket_active_ = false;
            return true;
        case 0x8C:
            *value = shared_.mfunc;
            return true;
        case 0x90:
            *value = (static_cast<uint32_t>(diagnostic) << 24) | (static_cast<uint32_t>(shared_.device_control) << 16) |
                     (static_cast<uint32_t>(card_control_) << 8) | shared_.retry_status;
            return true;
        case 0x94:
        case 0x98:
            *value = socket_dma_[(reg - 0x94u) >> 2];
            return true;
        case 0xA0:
            *value = kPowerManagementCaps;
            return true;
        case 0xA4:
            *value = kPmcsrBridgeSupport | pmcsr_;
            return true;
        case 0xA8:
            *value = (static_cast<uint32_t>(gpe_enable_) << 16) | gpe_status_;
            return true;
        case 0xAC:
            *value = (static_cast<uint32_t>(gpo_) << 16) | (shared_.gpi_levels & kGpiDataMask);
            return true;
        case 0xB0:
            *value = shared_.serial_bus;
            return true;
        default:
            return false;
    }
}

const char* Pci1225ConfigSpace::Write(uint32_t reg, uint32_t value) {
    if (ReservedRegister(reg))
        return nullptr;
    switch (reg) {
        case 0x04:
            command_ = static_cast<uint16_t>(value & kCommandWritable);
            return nullptr;
        case 0x0C:
            cache_line_ = static_cast<uint8_t>(value);
            latency_    = static_cast<uint8_t>(value >> 8);
            return nullptr;
        case 0x10:
            socket_bar_ = value & kSocketBarWritable;
            return nullptr;
        case 0x18:
            bus_numbers_ = value;
            return nullptr;
        case 0x1C:
        case 0x24:
            mem_base_[WindowIndex(reg, 0x1Cu)] = value & kMemWindowWritable;
            return nullptr;
        case 0x20:
        case 0x28:
            mem_limit_[WindowIndex(reg, 0x20u)] = value & kMemWindowWritable;
            return nullptr;
        case 0x2C:
        case 0x34:
            io_base_[WindowIndex(reg, 0x2Cu)] = value & kIoBaseWritable;
            return nullptr;
        case 0x30:
        case 0x38:
            io_limit_[WindowIndex(reg, 0x30u)] = value & kIoLimitWritable;
            return nullptr;
        case 0x3C: {
            const uint16_t bridge = static_cast<uint16_t>((value >> 16) & kBridgeControlWritable);
            int_line_            = static_cast<uint8_t>(value);
            bridge_control_      = static_cast<uint16_t>(bridge & ~kBridgeGlobal);
            shared_.bridge_global = static_cast<uint16_t>(bridge & kBridgeGlobal);
            return nullptr;
        }
        case 0x40:
            if ((shared_.system_global & kSysSubsysReadOnly) == 0u)
                shared_.subsystem = value;
            return nullptr;
        case 0x44:
            shared_.legacy_base = value & ~kLegacyBaseReadOne;
            return nullptr;
        case 0x80:
            if ((value & (kSysPcPciDreq | kSysPcPciDma)) != 0u)
                return "enables PC/PCI DMA, which needs a PC/PCI DMA chipset";
            if ((value & kSysVccProtectOff) != 0u)
                return "disables VCC protection, whose effect SCPS035B Table 24 does not give";
            shared_.system_global = value & kSysGlobalWritable;
            system_socket_ = (value & kSysSocketWritable & ~kSysSmiStatus) |
                             (system_socket_ & kSysSmiStatus & ~value);
            return nullptr;
        case 0x8C:
            if (!Pci1225InterruptTerminals::RoutingModeled(value & kMfuncWritable))
                return "selects a multifunction terminal function that is not modeled (GPO, LOCK, "
                       "PC/PCI DMA, zoomed video, CAUDPWM, LED, RI_OUT, GPE or CLKRUN)";
            shared_.mfunc = value & kMfuncWritable;
            return nullptr;
        case 0x90: {
            const uint8_t cc = static_cast<uint8_t>(value >> 8);
            const uint8_t dc = static_cast<uint8_t>(value >> 16);
            const uint8_t diag = static_cast<uint8_t>(value >> 24);
            if ((cc & kCcZoomVideo) != 0u)
                return "enables zoomed video, which takes the card terminals";
            if ((cc & kCcRingIndicateEnable) != 0u)
                return "enables RI_OUT, whose board connection is not modeled";
            if ((dc & kDcTestBits) != 0u)
                return "sets a device-control test bit that only 0 may be written to";
            if ((dc & kDc3vCapable) == 0u)
                return "clears the 3-V socket capable force, whose effect SCPS035B Table 28 does not give";
            shared_.retry_status      = static_cast<uint8_t>(value & kRetryWritable);
            card_control_             = static_cast<uint8_t>((cc & kCardControlWritable) |
                                                             (card_control_ & kCcIfg & ~cc));
            shared_.device_control    = static_cast<uint8_t>(dc & kDeviceControlWritable);
            shared_.diagnostic_global = static_cast<uint8_t>(diag & kDiagGlobal);
            diagnostic_               = static_cast<uint8_t>(diag & ~kDiagGlobal);
            return nullptr;
        }
        case 0x94:
            socket_dma_[0] = value & kSocketDma0Writable;
            return nullptr;
        case 0x98:
            if ((value & kSocketDmaDecode) != 0u)
                return "enables the distributed DMA register decode, which needs a DMA engine";
            socket_dma_[1] = value & kSocketDma1Writable;
            return nullptr;
        case 0xA4:
            if ((value & kPmcsrPmeEnable) != 0u)
                return "sets PME_EN, whose PME# events and terminal connection are not modeled";
            if ((pmcsr_ & kPmcsrPowerState) == kPmcsrD3Hot && (value & kPmcsrPowerState) == kPmcsrD0)
                return "returns the function from D3hot to D0, whose internal reset is not modeled";
            pmcsr_ = value & kPmcsrWritable;
            return nullptr;
        case 0xA8:
            gpe_enable_ = static_cast<uint16_t>(value >> 16);
            gpe_status_ = static_cast<uint16_t>(gpe_status_ & ~value);
            return nullptr;
        case 0xAC:
            gpo_ = static_cast<uint8_t>((value >> 16) & kGpoWritable);
            return nullptr;
        case 0xB0:
            shared_.serial_bus = (value & (kSerialDataIndexSlave | kSerialControlWritable)) |
                                 (shared_.serial_bus & kSerialControlClear & ~value);
            return nullptr;
        case 0x00:
        case 0x08:
        case 0x14:
        case 0xA0:
            return nullptr;
        default:
            return "is outside Table 18";
    }
}

void Pci1225ConfigSpace::SaveState(StateWriter& w) const {
    w.Write("cfg_command", command_);
    w.Write("cfg_cache_line", cache_line_);
    w.Write("cfg_latency", latency_);
    w.Write("cfg_socket_bar", socket_bar_);
    w.Write("cfg_bus_numbers", bus_numbers_);
    w.WriteBytes("cfg_mem_base", mem_base_, sizeof(mem_base_));
    w.WriteBytes("cfg_mem_limit", mem_limit_, sizeof(mem_limit_));
    w.WriteBytes("cfg_io_base", io_base_, sizeof(io_base_));
    w.WriteBytes("cfg_io_limit", io_limit_, sizeof(io_limit_));
    w.Write("cfg_int_line", int_line_);
    w.Write("cfg_bridge_control", bridge_control_);
    w.Write("cfg_system_socket", system_socket_);
    w.Write("cfg_card_control", card_control_);
    w.Write("cfg_diagnostic", diagnostic_);
    w.WriteBytes("cfg_socket_dma", socket_dma_, sizeof(socket_dma_));
    w.Write("cfg_pmcsr", pmcsr_);
    w.Write("cfg_gpe_enable", gpe_enable_);
    w.Write("cfg_gpe_status", gpe_status_);
    w.Write("cfg_gpo", gpo_);
    w.Write<uint8_t>("cfg_socket_active", socket_active_ ? 1u : 0u);
}

void Pci1225ConfigSpace::RestoreState(StateReader& r) {
    r.Read("cfg_command", command_);
    r.Read("cfg_cache_line", cache_line_);
    r.Read("cfg_latency", latency_);
    r.Read("cfg_socket_bar", socket_bar_);
    r.Read("cfg_bus_numbers", bus_numbers_);
    r.ReadBytes("cfg_mem_base", mem_base_, sizeof(mem_base_));
    r.ReadBytes("cfg_mem_limit", mem_limit_, sizeof(mem_limit_));
    r.ReadBytes("cfg_io_base", io_base_, sizeof(io_base_));
    r.ReadBytes("cfg_io_limit", io_limit_, sizeof(io_limit_));
    r.Read("cfg_int_line", int_line_);
    r.Read("cfg_bridge_control", bridge_control_);
    r.Read("cfg_system_socket", system_socket_);
    r.Read("cfg_card_control", card_control_);
    r.Read("cfg_diagnostic", diagnostic_);
    r.ReadBytes("cfg_socket_dma", socket_dma_, sizeof(socket_dma_));
    r.Read("cfg_pmcsr", pmcsr_);
    r.Read("cfg_gpe_enable", gpe_enable_);
    r.Read("cfg_gpe_status", gpe_status_);
    r.Read("cfg_gpo", gpo_);
    uint8_t active = 0u;
    r.Read("cfg_socket_active", active);
    socket_active_ = active != 0u;
}

}

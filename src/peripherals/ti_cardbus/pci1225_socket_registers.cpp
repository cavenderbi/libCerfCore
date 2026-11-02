#include "pci1225_socket_registers.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kSockEvent           = 0x00u;
constexpr uint32_t kSockMask            = 0x04u;
constexpr uint32_t kSockPresentState    = 0x08u;
constexpr uint32_t kSockForceEvent      = 0x0Cu;
constexpr uint32_t kSockControl         = 0x10u;
constexpr uint32_t kSockPowerManagement = 0x20u;
constexpr uint32_t kRegisterAlign       = 0x3u;

constexpr uint32_t kMaskCardDetect  = 0x06u;

constexpr uint32_t kForceEventRead     = 0x0000u;
constexpr uint32_t kForceCvsTest       = 0x4000u;
constexpr uint32_t kForcePresentSet    = 0x3FB0u;
constexpr uint32_t kForceCardVoltages  = 0x3C00u;

constexpr uint32_t kControlWritable = 0xF7u;
constexpr uint32_t kControlVccShift = 4u;
constexpr uint32_t kControlField    = 0x07u;
constexpr uint32_t kControlOff      = 0u;
constexpr uint32_t kControlVcc5V    = 2u;
constexpr uint32_t kControlVcc3V    = 3u;
constexpr uint32_t kControlXV       = 4u;
constexpr uint32_t kControlYV       = 5u;
constexpr uint32_t kControlVpp12    = 1u;
constexpr uint32_t kControlRequests = (kControlField << kControlVccShift) | kControlField;

constexpr uint32_t kPowerMgmtWritable = 0x00010001u;
constexpr uint32_t kPowerMgmtAccess   = 0x02000000u;

constexpr uint8_t kIfsBvd1  = 0x01u;
constexpr uint8_t kIfsReady = 0x20u;

constexpr uint32_t kSpsCardSts    = 0x01u;
constexpr uint32_t kSpsCcd1       = 0x02u;
constexpr uint32_t kSpsCcd2       = 0x04u;
constexpr uint32_t kSpsPowerCycle = 0x08u;
constexpr uint32_t kSps16BitCard  = 0x10u;
constexpr uint32_t kSpsReady      = 0x40u;
constexpr uint32_t kSpsBadVccReq  = 0x200u;
constexpr uint32_t kSps5VCard     = 0x400u;
constexpr uint32_t kSps3VCard     = 0x800u;
constexpr uint32_t kSpsXVCard     = 0x1000u;
constexpr uint32_t kSpsYVCard     = 0x2000u;
constexpr uint32_t kSps5VSocket   = 0x10000000u;
constexpr uint32_t kSps3VSocket   = 0x20000000u;

bool ReservedVcc(uint32_t code) {
    return code != kControlOff && code != kControlVcc5V && code != kControlVcc3V &&
           code != kControlXV && code != kControlYV;
}

bool ReservedVpp(uint32_t code) {
    return code > kControlYV;
}

uint32_t ByteLanes(uint32_t off, unsigned size) {
    const uint32_t lanes = size >= 4u ? 0xFFFFFFFFu : ((1u << (8u * size)) - 1u);
    return lanes << (8u * (off & kRegisterAlign));
}

uint32_t LaneValue(uint32_t off, uint32_t value) {
    return value << (8u * (off & kRegisterAlign));
}

}

bool Pci1225SocketRegisters::IsControl(uint32_t off) {
    return (off & ~kRegisterAlign) == kSockControl;
}

bool Pci1225SocketRegisters::ReadWindow(uint32_t off, unsigned size, const Pci1225CardPins& pins,
                                        uint32_t* value) {
    uint32_t reg = 0u;
    if (!Read(off & ~kRegisterAlign, pins, &reg))
        return false;
    *value = (reg & ByteLanes(off, size)) >> (8u * (off & kRegisterAlign));
    return true;
}

uint32_t Pci1225SocketRegisters::MergedControl(uint32_t off, uint32_t value, unsigned size) const {
    const uint32_t lanes = ByteLanes(off, size);
    return (control_ & ~lanes) | (LaneValue(off, value) & lanes);
}

const char* Pci1225SocketRegisters::WriteWindow(uint32_t off, uint32_t value, unsigned size) {
    const uint32_t lanes = ByteLanes(off, size);
    const uint32_t shifted = LaneValue(off, value) & lanes;
    switch (off & ~kRegisterAlign) {
        case kSockEvent:
            event_ &= ~shifted;
            return nullptr;
        case kSockMask:
            return WriteMask((mask_ & ~lanes) | shifted) ? nullptr : "selects a reserved CDMASK code";
        case kSockPresentState:
            return nullptr;
        case kSockForceEvent:
            WriteForceEvent(shifted);
            return nullptr;
        case kSockPowerManagement:
            power_mgmt_ = ((power_mgmt_ & ~lanes) | shifted) & kPowerMgmtWritable;
            return nullptr;
        default:
            return "is a reserved or unlisted offset of Table 57";
    }
}

void Pci1225SocketRegisters::Reset() {
    event_         = 0u;
    mask_          = 0u;
    control_       = 0u;
    power_mgmt_    = 0u;
    forced_        = 0u;
    bad_vcc_       = false;
    socket_access_ = false;
}

uint32_t Pci1225SocketRegisters::CardVoltageBits(const Pci1225CardPins& pins) {
    const bool vs1 = pins.vs1_grounded;
    const bool vs2 = pins.vs2_grounded;
    const bool lv  = pins.low_voltage_key;
    if (!vs1 && !vs2) return kSps5VCard;
    if (vs1 && !vs2) return lv ? kSps3VCard : (kSps5VCard | kSps3VCard);
    if (vs1 && vs2) return lv ? (kSps3VCard | kSpsXVCard) : (kSps5VCard | kSps3VCard | kSpsXVCard);
    return kSpsYVCard;
}

uint32_t Pci1225SocketRegisters::PresentState(const Pci1225CardPins& pins) const {
    uint32_t v = kSps5VSocket | kSps3VSocket | forced_ | (bad_vcc_ ? kSpsBadVccReq : 0u);
    if (!pins.present) return v | kSpsCcd1 | kSpsCcd2;
    v |= kSps16BitCard | CardVoltageBits(pins);
    if (pins.interface_status & kIfsReady) v |= kSpsReady;
    if (pins.interface_status & kIfsBvd1) v |= kSpsCardSts;
    if (pins.powered) v |= kSpsPowerCycle;
    return v;
}

bool Pci1225SocketRegisters::Read(uint32_t reg, const Pci1225CardPins& pins, uint32_t* value) {
    switch (reg) {
        case kSockEvent:
            *value = event_;
            return true;
        case kSockMask:
            *value = mask_;
            return true;
        case kSockPresentState:
            *value = PresentState(pins);
            return true;
        case kSockForceEvent:
            *value = kForceEventRead;
            return true;
        case kSockControl:
            *value = control_;
            return true;
        case kSockPowerManagement:
            *value = power_mgmt_ | (socket_access_ ? kPowerMgmtAccess : 0u);
            socket_access_ = false;
            return true;
        default:
            return false;
    }
}

bool Pci1225SocketRegisters::WriteMask(uint32_t value) {
    const uint32_t cd = value & kMaskCardDetect;
    if (cd != 0u && cd != kMaskCardDetect) return false;
    mask_ = value & kEventsAll;
    return true;
}

void Pci1225SocketRegisters::WriteForceEvent(uint32_t value) {
    if ((value & kForceCvsTest) != 0u)
        forced_ = 0u;
    forced_ |= value & kForcePresentSet;
    event_ |= value & kEventsAll;
}

bool Pci1225SocketRegisters::WriteControl(uint32_t value, Pci1225PowerRequest* request) {
    const uint32_t vcc = (value >> kControlVccShift) & kControlField;
    const uint32_t vpp = value & kControlField;
    if (ReservedVcc(vcc) || ReservedVpp(vpp)) return false;
    const uint32_t before = control_;
    control_ = value & kControlWritable;
    request->request_changed = false;
    request->vpp12_crossed = false;
    request->vcc_on = (before >> kControlVccShift & kControlField) == kControlVcc5V ||
                      (before >> kControlVccShift & kControlField) == kControlVcc3V;
    if ((forced_ & kForceCardVoltages) != 0u)
        return true;
    bad_vcc_ = vcc == kControlXV || vcc == kControlYV || vpp == kControlXV || vpp == kControlYV;
    request->vcc_on = !bad_vcc_ && (vcc == kControlVcc5V || vcc == kControlVcc3V);
    request->request_changed = ((before ^ control_) & kControlRequests) != 0u;
    request->vpp12_crossed = ((before & kControlField) == kControlVpp12) != (vpp == kControlVpp12);
    return true;
}

void Pci1225SocketRegisters::SaveState(StateWriter& w) const {
    w.Write("socket_event", event_);
    w.Write("socket_mask", mask_);
    w.Write("socket_control", control_);
    w.Write("socket_power_mgmt", power_mgmt_);
    w.Write("socket_forced", forced_);
    w.Write<uint8_t>("socket_bad_vcc", bad_vcc_ ? 1u : 0u);
    w.Write<uint8_t>("socket_access", socket_access_ ? 1u : 0u);
}

void Pci1225SocketRegisters::RestoreState(StateReader& r) {
    r.Read("socket_event", event_);
    r.Read("socket_mask", mask_);
    r.Read("socket_control", control_);
    r.Read("socket_power_mgmt", power_mgmt_);
    r.Read("socket_forced", forced_);
    uint8_t bad = 0u;
    r.Read("socket_bad_vcc", bad);
    bad_vcc_ = bad != 0u;
    uint8_t access = 0u;
    r.Read("socket_access", access);
    socket_access_ = access != 0u;
}

}

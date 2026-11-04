#include "pcic82365.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

enum : uint8_t {
    kRegInterfaceStatus       = 0x01,
    kRegPowerControl          = 0x02,
    kRegInterruptAndGeneral   = 0x03,
    kRegCardStatusChange      = 0x04,
    kRegStatusChangeIntConfig = 0x05,
};

/* REG_INTERFACE_STATUS bits (i82365reg.h PCIC_IF_STATUS_*). */
constexpr uint8_t kIfsBvd1      = 0x01u;
constexpr uint8_t kIfsBvd2      = 0x02u;
constexpr uint8_t kIfsCd1       = 0x04u;
constexpr uint8_t kIfsCd2       = 0x08u;
constexpr uint8_t kIfsWriteProtect = 0x10u;
constexpr uint8_t kIfsCardReady = 0x20u;
constexpr uint8_t kIfsCardInputs = 0x3Fu;

/* Interface Status bit 6 Card Power On per Power Control bit 4 VCC Power and bit 5 Auto-Power
   (CL-PD6710/'22 data sheet 6.2 p38, Table 6-1 p40; i82365reg.h PCIC_IF_STATUS_POWERACTIVE). */
constexpr uint8_t kIfsCardPowerOn = 0x40u;
constexpr uint8_t kPwrVccPower    = 0x10u;
constexpr uint8_t kPwrAutoPower   = 0x20u;

/* REG_INTERRUPT_AND_GENERAL_CONTROL CARDTYPE (i82365reg.h PCIC_INTR_CARDTYPE_IO). */
constexpr uint8_t kIntrCardTypeIo = 0x20u;
constexpr uint8_t kIntrCardReset  = 0x40u;
constexpr uint8_t kIntrCscToIntr  = 0x10u;
constexpr uint8_t kIntrRingEnable = 0x80u;

constexpr uint8_t kCscBatteryDead    = 0x01u;
constexpr uint8_t kCscBatteryWarning = 0x02u;
constexpr uint8_t kCscReadyChange    = 0x04u;
constexpr uint8_t kCscDetectChange   = 0x08u;
constexpr uint8_t kCscGpiChange      = 0x10u;
constexpr uint8_t kCscMemoryOnly   = 0x06u;
constexpr uint8_t kCscSourceMask   = 0x0Fu;
constexpr uint8_t kCscStatusMask   = kCscSourceMask | kCscGpiChange;

constexpr uint8_t kPwrOutputEnable = 0x80u;

}  /* namespace */

Pcic82365::SocketDrive Pcic82365::Drive() const {
    SocketDrive d;
    const bool auto_power = rules_.auto_power && (reg_power_control_ & kPwrAutoPower) != 0u;
    d.vcc = rules_.socket_control_power
        ? vcc_request_
        : (reg_power_control_ & kPwrVccPower) != 0u && (!auto_power || card_present_) &&
              (!rules_.vcc_requires_card || card_present_);
    d.outputs = (reg_power_control_ & kPwrOutputEnable) != 0u &&
                (!rules_.outputs_require_card || card_present_) && !isolated_;
    d.reset_released = d.outputs && (reg_interrupt_and_gen_ctrl_ & kIntrCardReset) != 0u;
    return d;
}

void Pcic82365::ReportDrive(const SocketDrive& before, ExcaWriteEffect* effect) const {
    const SocketDrive now = Drive();
    effect->power_on        = now.vcc;
    effect->power_changed   = now.vcc != before.vcc;
    effect->outputs_on      = now.outputs;
    effect->outputs_changed = now.outputs != before.outputs;
    effect->reset_changed   = now.reset_released != before.reset_released;
    effect->reset_asserted  = !now.reset_released;
}

bool Pcic82365::IoCardInterface() const {
    return (reg_interrupt_and_gen_ctrl_ & kIntrCardTypeIo) != 0u;
}

bool Pcic82365::ReadyPin() const {
    if (isolated_) return (frozen_inputs_ & kIfsCardReady) != 0u;
    return Drive().vcc ? !ready_ireq_low_ : rules_.card_inputs_high_without_vcc;
}

uint8_t Pcic82365::StatusChangeMask() const {
    uint8_t mask = kCscStatusMask;
    if (IoCardInterface()) mask = static_cast<uint8_t>(mask & ~kCscMemoryOnly);
    if (IoCardInterface() && rules_.ring_indicate_masks_status_change &&
        (reg_interrupt_and_gen_ctrl_ & kIntrRingEnable) != 0u)
        mask = static_cast<uint8_t>(mask & ~kCscBatteryDead);
    return mask;
}

void Pcic82365::LatchIfEnabled(uint8_t bit) {
    if (!rules_.latch_requires_enable || (reg_status_change_int_cfg_ & bit) != 0u)
        reg_card_status_change_ |= bit;
}

void Pcic82365::TrackPins() {
    const bool pin = ReadyPin();
    if (pin != ready_pin_) {
        ready_pin_ = pin;
        if (!IoCardInterface() && (pin || rules_.ready_change_any_edge)) LatchIfEnabled(kCscReadyChange);
    }
    const uint8_t bvd = static_cast<uint8_t>(InterfaceStatus() & (kIfsBvd1 | kIfsBvd2));
    if (bvd != bvd_pins_) {
        LatchBatteryChanges(bvd_pins_, bvd);
        bvd_pins_ = bvd;
    }
}

void Pcic82365::LatchBatteryChanges(uint8_t bvd_before, uint8_t bvd_now) {
    const bool bvd1_was = (bvd_before & kIfsBvd1) != 0u;
    const bool bvd1_now = (bvd_now & kIfsBvd1) != 0u;
    const bool bvd2_was = (bvd_before & kIfsBvd2) != 0u;
    const bool bvd2_now = (bvd_now & kIfsBvd2) != 0u;
    if (IoCardInterface()) {
        if (bvd1_was != bvd1_now && (!bvd1_now || rules_.stschg_change_any_edge) &&
            (StatusChangeMask() & kCscBatteryDead) != 0u)
            LatchIfEnabled(kCscBatteryDead);
        return;
    }
    const bool dead_pin_was = rules_.battery_dead_pin == ExcaBatteryDeadPin::kBvd1Low ? bvd1_was : bvd2_was;
    const bool dead_pin_now = rules_.battery_dead_pin == ExcaBatteryDeadPin::kBvd1Low ? bvd1_now : bvd2_now;
    if (dead_pin_was && !dead_pin_now) LatchIfEnabled(kCscBatteryDead);
    bool warning_entered = false;
    switch (rules_.battery_warning) {
        case ExcaBatteryWarning::kBvd2Fall:
            warning_entered = bvd2_was && !bvd2_now;
            break;
        case ExcaBatteryWarning::kEnterBvd1HighBvd2Low:
            warning_entered = (bvd1_now && !bvd2_now) && !(bvd1_was && !bvd2_was);
            break;
        case ExcaBatteryWarning::kEnterBvd2HighBvd1Low:
            warning_entered = (bvd2_now && !bvd1_now) && !(bvd2_was && !bvd1_was);
            break;
    }
    if (warning_entered) LatchIfEnabled(kCscBatteryWarning);
}

void Pcic82365::CardDetectChanged(bool present, ExcaWriteEffect* effect) {
    pin_present_ = present;
    if (isolated_) return;
    const SocketDrive drive_before = Drive();
    ApplyCardDetect(present);
    ReportDrive(drive_before, effect);
    TrackPins();
}

void Pcic82365::ApplyCardDetect(bool present) {
    card_present_ = present;
    if (!present) {
        if (rules_.removal_resets_power_control) reg_power_control_ = 0u;
        const uint8_t fields = rules_.removal_resets_interrupt_control;
        reg_interrupt_and_gen_ctrl_ = static_cast<uint8_t>((reg_interrupt_and_gen_ctrl_ & ~fields) |
                                                           (rules_.interrupt_control_reset & fields));
        if (rules_.removal_resets_window_enable) windows_.DisableWindows();
        if (configuration_reset_on_removal_) ResetConfiguration();
    }
    LatchIfEnabled(kCscDetectChange);
}

void Pcic82365::SetInterfaceIsolated(bool on, ExcaWriteEffect* effect) {
    if (on == isolated_) return;
    const SocketDrive drive_before = Drive();
    if (on) frozen_inputs_ = static_cast<uint8_t>(InterfaceStatus() & kIfsCardInputs);
    isolated_ = on;
    if (!on) {
        if (pin_present_ != card_present_) ApplyCardDetect(pin_present_);
        ready_ireq_low_ = pin_ireq_low_;
    }
    ReportDrive(drive_before, effect);
    TrackPins();
}

void Pcic82365::ResetConfiguration() {
    reg_interrupt_and_gen_ctrl_ = static_cast<uint8_t>((reg_interrupt_and_gen_ctrl_ & kIntrCscToIntr) |
                                                       rules_.interrupt_control_reset);
    windows_.ResetConfiguration();
}

void Pcic82365::SoftwareCardDetect() {
    if ((reg_status_change_int_cfg_ & kCscDetectChange) != 0u)
        reg_card_status_change_ |= kCscDetectChange;
}

void Pcic82365::LatchGpiChange() {
    reg_card_status_change_ |= kCscGpiChange;
}

void Pcic82365::ClearGpiChange() {
    reg_card_status_change_ = static_cast<uint8_t>(reg_card_status_change_ & ~kCscGpiChange);
}

void Pcic82365::RequestVccFromSocketControl(bool vcc_on, ExcaWriteEffect* effect) {
    const SocketDrive drive_before = Drive();
    vcc_request_ = vcc_on;
    ReportDrive(drive_before, effect);
    TrackPins();
}

void Pcic82365::SetReadyIreqLow(bool low) {
    pin_ireq_low_ = low;
    if (isolated_) return;
    ready_ireq_low_ = low;
    TrackPins();
}

uint8_t Pcic82365::InterfaceStatus() const {
    if (isolated_)
        return static_cast<uint8_t>(frozen_inputs_ | (Drive().vcc ? kIfsCardPowerOn : 0u));
    uint8_t v = 0u;
    if (card_present_)
        v |= kIfsCd1 | kIfsCd2;
    const bool vcc = Drive().vcc;
    if (vcc)
        v |= kIfsCardPowerOn;
    if (vcc || rules_.card_inputs_high_without_vcc) {
        v |= kIfsBvd1 | kIfsBvd2;
        if (!card_present_ || !vcc)
            v |= kIfsWriteProtect;
    }
    if (ReadyPin())
        v |= kIfsCardReady;
    return v;
}

uint8_t Pcic82365::ActiveStatusChangeBits() const {
    const uint8_t enabled = static_cast<uint8_t>((reg_status_change_int_cfg_ & kCscSourceMask) | kCscGpiChange);
    return static_cast<uint8_t>(reg_card_status_change_ & enabled & StatusChangeMask());
}

void Pcic82365::Reset() {
    vcc_request_                = false;
    reg_power_control_          = 0u;
    reg_interrupt_and_gen_ctrl_ = rules_.interrupt_control_reset;
    reg_card_status_change_     = 0u;
    reg_status_change_int_cfg_  = 0u;
    windows_.Reset();
    isolated_       = false;
    frozen_inputs_  = 0u;
    card_present_   = pin_present_;
    ready_ireq_low_ = pin_ireq_low_;
    ready_pin_ = ReadyPin();
    bvd_pins_ = static_cast<uint8_t>(InterfaceStatus() & (kIfsBvd1 | kIfsBvd2));
}

bool Pcic82365::ReservedPowerCode(uint8_t value) const {
    const bool a = rules_.power_reserved_field_a != 0u &&
                   (value & rules_.power_reserved_field_a) == rules_.power_reserved_code_a;
    const bool b = rules_.power_reserved_field_b != 0u &&
                   (value & rules_.power_reserved_field_b) == rules_.power_reserved_code_b;
    return a || b;
}

bool Pcic82365::ReservedIrqSelect(uint8_t code) const {
    return ((rules_.irq_select_reserved >> code) & 1u) != 0u;
}

bool Pcic82365::Owns(uint8_t index) const {
    if (index >= kRegInterfaceStatus && index <= kRegStatusChangeIntConfig) return true;
    return windows_.Owns(index);
}

bool Pcic82365::ReadReg(uint8_t index, uint8_t* value) {
    if (windows_.Read(index, value)) return true;
    switch (index) {
        case kRegInterfaceStatus:
            *value = InterfaceStatus();
            return true;
        case kRegPowerControl:
            *value = reg_power_control_;
            return true;
        case kRegInterruptAndGeneral:
            *value = reg_interrupt_and_gen_ctrl_;
            return true;
        case kRegCardStatusChange:
            *value = ReadCardStatusChange();
            return true;
        case kRegStatusChangeIntConfig:
            *value = reg_status_change_int_cfg_;
            return true;
        default:
            return false;
    }
}

uint8_t Pcic82365::ReadCardStatusChange() {
    uint8_t v = static_cast<uint8_t>(reg_card_status_change_ & StatusChangeMask());
    if (rules_.status_read_masked_by_enable)
        v = static_cast<uint8_t>(v & reg_status_change_int_cfg_ & kCscSourceMask);
    if (!explicit_status_clear_)
        reg_card_status_change_ = 0u;
    return v;
}

bool Pcic82365::WriteReg(uint8_t index, uint8_t value, ExcaWriteEffect* effect) {
    const SocketDrive drive_before = Drive();
    if (!StoreReg(index, value)) return false;
    ReportDrive(drive_before, effect);
    TrackPins();
    return true;
}

bool Pcic82365::StoreReg(uint8_t index, uint8_t value) {
    if (windows_.Store(index, value)) return true;
    switch (index) {
        case kRegInterfaceStatus:
            return true;
        case kRegPowerControl:
            if (ReservedPowerCode(value)) return false;
            if (rules_.reserved_bits_halt && (value & rules_.power_control_reserved) != 0u) return false;
            reg_power_control_ = static_cast<uint8_t>(value & ~rules_.power_control_reserved);
            if (rules_.socket_control_power) vcc_request_ = (reg_power_control_ & kPwrVccPower) != 0u;
            return true;
        case kRegInterruptAndGeneral:
            if (ReservedIrqSelect(static_cast<uint8_t>(value & kCardIrqSelectMask))) return false;
            if (rules_.reserved_bits_halt && (value & rules_.interrupt_control_reserved) != 0u) return false;
            reg_interrupt_and_gen_ctrl_ = static_cast<uint8_t>(value & ~rules_.interrupt_control_reserved);
            return true;
        case kRegStatusChangeIntConfig:
            if (ReservedIrqSelect(static_cast<uint8_t>(value >> kCscIrqSelectShift))) return false;
            reg_status_change_int_cfg_ = value;
            return true;
        case kRegCardStatusChange:
            if (rules_.reserved_bits_halt && (value & rules_.status_change_reserved) != 0u) return false;
            if (explicit_status_clear_)
                reg_card_status_change_ = static_cast<uint8_t>(reg_card_status_change_ & ~(value & kCscStatusMask));
            return true;
        default:
            return false;
    }
}

void Pcic82365::SaveState(StateWriter& w) const {
    w.Write<uint8_t>("card_present", card_present_ ? 1u : 0u);
    w.Write<uint8_t>("ready_ireq_low", ready_ireq_low_ ? 1u : 0u);
    w.Write<uint8_t>("ready_pin", ready_pin_ ? 1u : 0u);
    w.Write("bvd_pins", bvd_pins_);
    w.Write<uint8_t>("vcc_request", vcc_request_ ? 1u : 0u);
    w.Write("reg_power_control", reg_power_control_);
    w.Write("reg_interrupt_and_gen_ctrl", reg_interrupt_and_gen_ctrl_);
    w.Write("reg_card_status_change", reg_card_status_change_);
    w.Write("reg_status_change_int_cfg", reg_status_change_int_cfg_);
    windows_.SaveState(w);
    w.Write<uint8_t>("pin_present", pin_present_ ? 1u : 0u);
    w.Write<uint8_t>("pin_ireq_low", pin_ireq_low_ ? 1u : 0u);
    w.Write<uint8_t>("isolated", isolated_ ? 1u : 0u);
    w.Write("frozen_inputs", frozen_inputs_);
}

void Pcic82365::RestoreState(StateReader& r) {
    uint8_t present = 0u;
    r.Read("card_present", present);
    card_present_ = present != 0u;
    uint8_t ready_ireq_low = 0u;
    r.Read("ready_ireq_low", ready_ireq_low);
    ready_ireq_low_ = ready_ireq_low != 0u;
    uint8_t pin = 0u;
    r.Read("ready_pin", pin);
    ready_pin_ = pin != 0u;
    r.Read("bvd_pins", bvd_pins_);
    uint8_t vcc = 0u;
    r.Read("vcc_request", vcc);
    vcc_request_ = vcc != 0u;
    r.Read("reg_power_control", reg_power_control_);
    r.Read("reg_interrupt_and_gen_ctrl", reg_interrupt_and_gen_ctrl_);
    r.Read("reg_card_status_change", reg_card_status_change_);
    r.Read("reg_status_change_int_cfg", reg_status_change_int_cfg_);
    windows_.RestoreState(r);
    uint8_t pin_present = 0u;
    r.Read("pin_present", pin_present);
    pin_present_ = pin_present != 0u;
    uint8_t pin_ireq_low = 0u;
    r.Read("pin_ireq_low", pin_ireq_low);
    pin_ireq_low_ = pin_ireq_low != 0u;
    uint8_t isolated = 0u;
    r.Read("isolated", isolated);
    isolated_ = isolated != 0u;
    r.Read("frozen_inputs", frozen_inputs_);
}

}

#pragma once

#include "../pcmcia/pcmcia_io_width.h"
#include "pcic82365_window_registers.h"

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

struct ExcaWriteEffect {
    bool power_changed        = false;
    bool power_on             = false;
    bool outputs_changed      = false;
    bool outputs_on           = false;
    bool reset_changed        = false;
    bool reset_asserted       = false;

    static ExcaWriteEffect SocketOff() {
        ExcaWriteEffect fx;
        fx.power_changed = true;
        fx.outputs_changed = true;
        fx.reset_changed = true;
        fx.reset_asserted = true;
        return fx;
    }
    static ExcaWriteEffect Outputs(bool on) {
        ExcaWriteEffect fx;
        fx.outputs_changed = true;
        fx.outputs_on = on;
        return fx;
    }
};

enum class ExcaBatteryDeadPin : uint8_t { kBvd1Low, kBvd2Low };

enum class ExcaBatteryWarning : uint8_t { kBvd2Fall, kEnterBvd1HighBvd2Low, kEnterBvd2HighBvd1Low };

struct ExcaPartRules {
    bool ready_change_any_edge;
    bool latch_requires_enable;
    bool status_read_masked_by_enable;
    bool auto_power;
    uint8_t power_control_reserved;
    uint8_t power_reserved_field_a;
    uint8_t power_reserved_code_a;
    uint8_t power_reserved_field_b;
    uint8_t power_reserved_code_b;
    uint8_t window_enable_reserved;
    uint8_t memory_end_high_reserved;
    uint8_t io_offset_low_reserved;
    bool window_page_registers;
    bool socket_control_power;
    bool stschg_change_any_edge;
    ExcaBatteryDeadPin battery_dead_pin;
    ExcaBatteryWarning battery_warning;
    bool outputs_require_card;
    bool card_inputs_high_without_vcc;
    bool ring_indicate_masks_status_change;
    uint16_t irq_select_reserved;
};

class Pcic82365 {
public:
    explicit Pcic82365(ExcaPartRules rules)
        : rules_(rules),
          windows_({rules.window_enable_reserved, rules.memory_end_high_reserved,
                    rules.io_offset_low_reserved, rules.window_page_registers}) {}

    void CardDetectChanged(bool present, ExcaWriteEffect* effect);
    void RequestVccFromSocketControl(bool vcc_on, ExcaWriteEffect* effect);
    void SetReadyIreqLow(bool low);
    void SetInterfaceIsolated(bool on, ExcaWriteEffect* effect);
    void SoftwareCardDetect();
    void LatchGpiChange();
    void ClearGpiChange();
    void SetConfigurationResetOnRemoval(bool on) { configuration_reset_on_removal_ = on; }
    uint8_t InterfaceStatus() const;
    bool IreqPinLow() const { return ready_ireq_low_; }
    bool ReadyPin() const;
    bool FunctionalInterrupt() const { return ready_ireq_low_ && IoCardInterface(); }

    bool StatusChangeInterruptActive() const { return ActiveStatusChangeBits() != 0u; }
    uint8_t ActiveStatusChangeBits() const;

    static constexpr unsigned kCscIrqSelectShift = 4u;
    static constexpr uint8_t  kCardIrqSelectMask = 0x0Fu;

    uint8_t CscIrqSelect()  const { return static_cast<uint8_t>(reg_status_change_int_cfg_ >> kCscIrqSelectShift); }
    uint8_t CardIrqSelect() const { return static_cast<uint8_t>(reg_interrupt_and_gen_ctrl_ & kCardIrqSelectMask); }
    uint8_t InterruptAndGeneralControl() const { return reg_interrupt_and_gen_ctrl_; }
    uint8_t WindowEnable() const { return windows_.WindowEnable(); }
    bool    IoCardInterface() const;
    bool    OutputsEnabled() const { return Drive().outputs; }

    void SetExplicitStatusClear(bool on) { explicit_status_clear_ = on; }

    bool Owns(uint8_t index) const;

    bool    ReadReg(uint8_t index, uint8_t* value);
    bool    WriteReg(uint8_t index, uint8_t value, ExcaWriteEffect* effect);
    uint8_t PowerControl() const { return reg_power_control_; }

    void Reset();

    bool MapIo(uint32_t bus_io, uint32_t* card_io, PcmciaIoWidth* width) const {
        return windows_.MapIo(bus_io, card_io, width);
    }
    bool MapMem(uint32_t bus_off, uint32_t* card_addr,
                bool* attribute, bool* writable, bool* sixteen_bit) const {
        return windows_.MapMem(bus_off, card_addr, attribute, writable, sixteen_bit);
    }
    bool SixteenBitWindowDecodes(uint32_t bus_off, unsigned decode_shift) const {
        return windows_.SixteenBitWindowDecodes(bus_off, decode_shift);
    }

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    uint8_t ReadCardStatusChange();
    uint8_t StatusChangeMask() const;
    void    ApplyCardDetect(bool present);

    struct SocketDrive {
        bool vcc            = false;
        bool outputs        = false;
        bool reset_released = false;
    };
    SocketDrive Drive() const;
    void ReportDrive(const SocketDrive& before, ExcaWriteEffect* effect) const;
    void TrackPins();
    void LatchBatteryChanges(uint8_t bvd_before, uint8_t bvd_now);
    void LatchIfEnabled(uint8_t bit);
    void ResetConfiguration();
    bool StoreReg(uint8_t index, uint8_t value);
    bool ReservedPowerCode(uint8_t value) const;
    bool ReservedIrqSelect(uint8_t code) const;

    ExcaPartRules            rules_;
    Pcic82365WindowRegisters windows_;

    bool    card_present_               = false;
    bool    ready_ireq_low_             = false;
    bool    pin_present_                = false;
    bool    pin_ireq_low_               = false;
    bool    isolated_                   = false;
    uint8_t frozen_inputs_              = 0u;
    bool    ready_pin_                  = false;
    uint8_t bvd_pins_                   = 0u;
    bool    explicit_status_clear_      = false;
    bool    configuration_reset_on_removal_ = false;
    bool    vcc_request_                = false;
    uint8_t reg_power_control_          = 0u;
    uint8_t reg_interrupt_and_gen_ctrl_ = 0u;
    uint8_t reg_card_status_change_     = 0u;
    uint8_t reg_status_change_int_cfg_  = 0u;
};

}

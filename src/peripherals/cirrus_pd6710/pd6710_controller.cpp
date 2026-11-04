#include "pd6710_controller.h"

#include "pd6710_card_irq_line.h"
#include "pd6710_management_irq_line.h"

#include <cerfcore/boards/board_context.h>
#include "../../boards/samsung_smdk2410/samsung_smdk2410_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/host_widget_registry.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kPortIndex   = 0x0u;
constexpr uint8_t  kNoIrqSelect = 0x0u;

enum : uint8_t {
    kRegChipRevision    = 0x00,
    kRegGeneralControl  = 0x16,
    kRegFifoControl     = 0x17,
    kRegGlobalControl   = 0x1E,
    kRegChipInfo        = 0x1F,
    kRegAtaControl      = 0x26,
    kRegScratchpad      = 0x27,
    kRegExtendedIndex   = 0x2E,
    kRegExtendedData    = 0x2F,
    kRegSetupTiming0    = 0x3A,
    kRegCommandTiming0  = 0x3B,
    kRegRecoveryTiming0 = 0x3C,
    kRegSetupTiming1    = 0x3D,
    kRegCommandTiming1  = 0x3E,
    kRegRecoveryTiming1 = 0x3F,
};

constexpr uint8_t  kIndexDevice1          = 0x80u;
constexpr uint8_t  kIndexSocketB          = 0x40u;
constexpr uint8_t  kChipRevision          = 0x82u;
constexpr uint8_t  kIntrManageIntToIntr   = 0x10u;
constexpr uint8_t  kMiscCtrl1FiveVoltCard = 0x01u;
constexpr uint8_t  kMiscCtrl1PulseManage  = 0x04u;
constexpr uint8_t  kMiscCtrl1PulseSysIrq  = 0x08u;
constexpr uint8_t  kMiscCtrl1Writable     = 0xFEu;
constexpr uint8_t  kMiscCtrl1InpackEnable = 0x80u;
constexpr uint8_t  kFifoEmpty             = 0x80u;
constexpr uint8_t  kFifoScratch           = 0x7Fu;
constexpr uint8_t  kMiscCtrl2AfterReset   = 0x02u;
constexpr uint8_t  kMiscCtrl2Suspend      = 0x04u;
constexpr uint8_t  kMiscCtrl2ThreeStateBit7 = 0x20u;
constexpr uint8_t  kMiscCtrl2Reserved     = 0x40u;
constexpr uint8_t  kChipInfoAfterReset    = 0xCEu;
constexpr uint8_t  kChipInfoIdToggle      = 0xC0u;
constexpr uint8_t  kAtaMode               = 0x01u;
constexpr unsigned kAtaLineShift          = 3u;
constexpr uint32_t kAtaLineMask           = 0x1Fu;
constexpr unsigned kCardAddrLine21        = 21u;
constexpr uint8_t  kDataBit7              = 0x80u;
constexpr uint32_t kIoDecodeMask          = 0xFFFFu;
constexpr uint32_t kFloppyStatusA         = 0x3F7u;
constexpr uint32_t kFloppyStatusB         = 0x377u;

constexpr std::array<uint8_t, 6> kTimingAfterReset = { 0x01u, 0x06u, 0x03u, 0x01u, 0x0Fu, 0x03u };

constexpr ExcaPartRules kExcaRules = [] {
    ExcaPartRules rules{};
    rules.ready_change_any_edge = true;
    rules.latch_requires_enable = true;
    rules.status_read_masked_by_enable = false;
    rules.auto_power = true;
    rules.power_control_reserved = 0x00u;
    rules.power_reserved_field_a = 0x00u;
    rules.power_reserved_code_a = 0x00u;
    rules.power_reserved_field_b = 0x00u;
    rules.power_reserved_code_b = 0x00u;
    rules.window_enable_reserved = 0x00u;
    rules.memory_end_high_reserved = 0x00u;
    rules.io_offset_low_reserved = 0x00u;
    rules.window_page_registers = false;
    rules.socket_control_power = false;
    rules.stschg_change_any_edge = true;
    rules.battery_dead_pin = ExcaBatteryDeadPin::kBvd1Low;
    rules.battery_warning = ExcaBatteryWarning::kBvd2Fall;
    rules.outputs_require_card = true;
    rules.ring_indicate_masks_status_change = false;
    rules.irq_select_reserved = 0x2146u;
    rules.io_offset_registers = true;
    rules.memory_address_high_mask = 0x0Fu;
    return rules;
}();

bool NewInterruptBits(uint8_t now, uint8_t before) {
    return (now & ~before) != 0u;
}

}  /* namespace */

Pd6710Controller::Pd6710Controller(CerfEmulator& emu)
    : Service(emu), slot_(emu, *this, "PCMCIA #1"), exca_(kExcaRules) {}

bool Pd6710Controller::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SamsungSmdk2410;
}

void Pd6710Controller::OnReady() {
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        ResetRegistersLocked();
    }
    emu_.Get<HostWidgetRegistry>().Register(&slot_);
    sequencer_.Run([this] {
        return ExcaSocketUpdates{{ {&slot_, ExcaWriteEffect::SocketOff()} }};
    });
    auto& reset = emu_.Get<GuestCpuReset>();
    reset.RegisterResetListener([this](ResetLineKind) { ResetController(); });
    reset.RegisterResetReleaseListener([this] {
        std::lock_guard<std::mutex> lk(state_mutex_);
        DriveCardLineLocked(true);
    });
}

void Pd6710Controller::ResetController() {
    sequencer_.Run([this] {
        std::lock_guard<std::mutex> lk(state_mutex_);
        ResetRegistersLocked();
        return ExcaSocketUpdates{{ {&slot_, ExcaWriteEffect::SocketOff()} }};
    });
}

void Pd6710Controller::ResetRegistersLocked() {
    exca_.Reset();
    index_ = 0u;
    reg_chip_info_ = kChipInfoAfterReset;
    timing_ = kTimingAfterReset;
    misc_ctrl1_ = 0u;
    fifo_scratch_ = 0u;
    misc_ctrl2_ = kMiscCtrl2AfterReset;
    ata_control_ = 0u;
    scratchpad_ = 0u;
    intr_bits_ = 0u;
    irq_pin_bits_ = 0u;
    card_request_ = false;
    emu_.Get<Pd6710ManagementIrqLine>().SetLevel(false);
}

bool Pd6710Controller::ManagementToIntrLocked() const {
    return (exca_.InterruptAndGeneralControl() & kIntrManageIntToIntr) != 0u;
}

bool Pd6710Controller::PulseManagementLocked() const {
    return (misc_ctrl1_ & kMiscCtrl1PulseManage) != 0u;
}

bool Pd6710Controller::RoutedToWiredIrqLocked(const char* source, uint8_t select) const {
    auto& line = emu_.Get<Pd6710CardIrqLine>();
    if (select == line.WiredIrqSelect())
        return true;
    if (select != kNoIrqSelect && !line.IrqPinDrivesNothing(select))
        emu_.Get<Fatal>().Die("[PD6710] %s interrupt routed to IRQ%u, whose board connection is not modeled",
                              source, select);
    return false;
}

void Pd6710Controller::RouteManagementLocked(uint8_t* to_intr, uint8_t* to_irq_pin) const {
    const uint8_t active = exca_.ActiveStatusChangeBits();
    const bool to_pins = !ManagementToIntrLocked();
    const bool wired = to_pins && RoutedToWiredIrqLocked("management", exca_.CscIrqSelect());
    *to_intr    = to_pins ? 0u : active;
    *to_irq_pin = wired ? active : 0u;
}

void Pd6710Controller::UpdateManagementLineLocked() {
    uint8_t to_intr, to_irq_pin;
    RouteManagementLocked(&to_intr, &to_irq_pin);
    const bool pulse = PulseManagementLocked();
    const bool fire_intr = pulse && NewInterruptBits(to_intr, intr_bits_);
    const bool fire_pin  = pulse && NewInterruptBits(to_irq_pin, irq_pin_bits_);
    intr_bits_ = to_intr;
    irq_pin_bits_ = to_irq_pin;
    auto& management = emu_.Get<Pd6710ManagementIrqLine>();
    management.SetLevel(!pulse && to_intr != 0u);
    if (fire_intr) management.Pulse();
    if (fire_pin && !card_line_level_) {
        auto& line = emu_.Get<Pd6710CardIrqLine>();
        line.Assert();
        line.Deassert();
    }
}

void Pd6710Controller::DriveCardLineLocked(bool force) {
    const bool wired = exca_.IoCardInterface() && RoutedToWiredIrqLocked("card", exca_.CardIrqSelect());
    const bool card = wired && exca_.FunctionalInterrupt();
    const bool pulse_card = (misc_ctrl1_ & kMiscCtrl1PulseSysIrq) != 0u;
    const bool card_edge = pulse_card && card && !card_request_;
    card_request_ = card;
    const bool level = (card && !pulse_card) || (!PulseManagementLocked() && irq_pin_bits_ != 0u);
    auto& line = emu_.Get<Pd6710CardIrqLine>();
    if (force || level != card_line_level_) {
        card_line_level_ = level;
        if (level) line.Assert();
        else       line.Deassert();
    }
    if (card_edge && !card_line_level_) {
        line.Assert();
        line.Deassert();
    }
}

bool Pd6710Controller::SuspendedLocked() const {
    return (misc_ctrl2_ & kMiscCtrl2Suspend) != 0u;
}

bool Pd6710Controller::InpackGated() {
    std::lock_guard<std::mutex> lk(state_mutex_);
    return (misc_ctrl1_ & kMiscCtrl1InpackEnable) != 0u;
}

void Pd6710Controller::RefuseIndexLocked(const char* access) {
    emu_.Get<Fatal>().Die("[PD6710] %s of INDEX 0x%02X, a register the CL-PD6710 does not have", access, index_);
}

uint32_t Pd6710Controller::MaskFloppyStatusBit7(uint32_t bus_io, uint32_t value, unsigned bytes) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    if ((misc_ctrl2_ & kMiscCtrl2ThreeStateBit7) == 0u) return value;
    for (unsigned lane = 0; lane < bytes; ++lane) {
        const uint32_t port = (bus_io + lane) & kIoDecodeMask;
        if (port != kFloppyStatusA && port != kFloppyStatusB) continue;
        const uint32_t bit = static_cast<uint32_t>(kDataBit7) << (8u * lane);
        const uint32_t floating = static_cast<uint32_t>(floating_.Read8("PD6710 data bit 7", port)) << (8u * lane);
        value = (value & ~bit) | (floating & bit);
    }
    return value;
}

uint8_t Pd6710Controller::ReadPcicByte(uint32_t port) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    if (port == kPortIndex) return index_;
    if ((index_ & kIndexDevice1) != 0u)
        return floating_.Read8("PD6710 device 1 data register", port);
    const uint8_t v = ReadIndexedDataLocked();
    UpdateManagementLineLocked();
    DriveCardLineLocked(false);
    return v;
}

void Pd6710Controller::WritePcicByte(uint32_t port, uint8_t value) {
    if (port == kPortIndex) {
        std::lock_guard<std::mutex> lk(state_mutex_);
        index_ = value;
        return;
    }
    sequencer_.Run([&] {
        ExcaSocketUpdates updates{};
        std::lock_guard<std::mutex> lk(state_mutex_);
        if ((index_ & kIndexDevice1) != 0u)
            return updates;
        WriteIndexedDataLocked(value, &updates[0].effect);
        updates[0].slot = &slot_;
        UpdateManagementLineLocked();
        DriveCardLineLocked(false);
        return updates;
    });
}

uint8_t Pd6710Controller::ReadIndexedDataLocked() {
    if ((index_ & kIndexSocketB) != 0u)
        RefuseIndexLocked("read");
    uint8_t owned = 0u;
    if (exca_.ReadReg(index_, &owned))
        return owned;
    switch (index_) {
        case kRegChipRevision:
            return kChipRevision;
        case kRegGeneralControl:
            return static_cast<uint8_t>(misc_ctrl1_ |
                ((slot_.Vs1Grounded() || slot_.Vs2Grounded()) ? 0u : kMiscCtrl1FiveVoltCard));
        case kRegFifoControl:
            return static_cast<uint8_t>(kFifoEmpty | fifo_scratch_);
        case kRegGlobalControl:
            return misc_ctrl2_;
        case kRegChipInfo: {
            const uint8_t v = reg_chip_info_;
            reg_chip_info_ = static_cast<uint8_t>(reg_chip_info_ ^ kChipInfoIdToggle);
            return v;
        }
        case kRegAtaControl:
            return ata_control_;
        case kRegScratchpad:
            return scratchpad_;
        case kRegExtendedIndex:
        case kRegExtendedData:
            RefuseIndexLocked("read");
        case kRegSetupTiming0:
        case kRegCommandTiming0:
        case kRegRecoveryTiming0:
        case kRegSetupTiming1:
        case kRegCommandTiming1:
        case kRegRecoveryTiming1:
            return timing_[index_ - kRegSetupTiming0];
        default:
            emu_.Get<Fatal>().Die("[PD6710] read of INDEX 0x%02X, outside the CL-PD6710 register map", index_);
    }
}

void Pd6710Controller::WriteIndexedDataLocked(uint8_t value, ExcaWriteEffect* effect) {
    if ((index_ & kIndexSocketB) != 0u)
        RefuseIndexLocked("write");
    if (exca_.Owns(index_)) {
        if (!exca_.WriteReg(index_, value, effect))
            emu_.Get<Fatal>().Die("[PD6710] INDEX 0x%02X value 0x%02X selects a reserved IRQ code", index_, value);
        return;
    }
    switch (index_) {
        case kRegGeneralControl:
            misc_ctrl1_ = static_cast<uint8_t>(value & kMiscCtrl1Writable);
            return;
        case kRegFifoControl:
            fifo_scratch_ = static_cast<uint8_t>(value & kFifoScratch);
            return;
        case kRegGlobalControl:
            if ((value & kMiscCtrl2Reserved) != 0u)
                emu_.Get<Fatal>().Die("[PD6710] Misc Control 2 value 0x%02X sets bit 6, reserved on the CL-PD6710",
                                      value);
            misc_ctrl2_ = value;
            return;
        case kRegChipInfo:
            reg_chip_info_ = kChipInfoAfterReset;
            return;
        case kRegAtaControl:
            ata_control_ = value;
            return;
        case kRegScratchpad:
            scratchpad_ = value;
            return;
        case kRegChipRevision:
            return;
        case kRegExtendedIndex:
        case kRegExtendedData:
            RefuseIndexLocked("write");
        case kRegSetupTiming0:
        case kRegCommandTiming0:
        case kRegRecoveryTiming0:
        case kRegSetupTiming1:
        case kRegCommandTiming1:
        case kRegRecoveryTiming1:
            timing_[index_ - kRegSetupTiming0] = value;
            return;
        default:
            emu_.Get<Fatal>().Die("[PD6710] write 0x%02X to INDEX 0x%02X, outside the CL-PD6710 register map",
                                  value, index_);
    }
}

bool Pd6710Controller::MapIo(uint32_t bus_io, uint32_t* card_io, PcmciaIoWidth* width) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    if (SuspendedLocked()) return false;
    return exca_.MapIo(bus_io, card_io, width);
}

bool Pd6710Controller::MapMem(uint32_t bus_off, uint32_t* card_addr,
                              bool* attribute, bool* writable, bool* sixteen_bit) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    if (SuspendedLocked()) return false;
    if (!exca_.MapMem(bus_off, card_addr, attribute, writable, sixteen_bit)) return false;
    if ((ata_control_ & kAtaMode) != 0u) {
        const uint32_t lines = (static_cast<uint32_t>(ata_control_) >> kAtaLineShift) & kAtaLineMask;
        *card_addr = (*card_addr & ~(kAtaLineMask << kCardAddrLine21)) | (lines << kCardAddrLine21);
    }
    return true;
}

void Pd6710Controller::OnCardDetectChanged(PcmciaSlot& slot) {
    sequencer_.Run([&] {
        ExcaSocketUpdates updates{};
        std::lock_guard<std::mutex> lk(state_mutex_);
        exca_.CardDetectChanged(slot.HasCard(), &updates[0].effect);
        updates[0].slot = &slot_;
        UpdateManagementLineLocked();
        DriveCardLineLocked(false);
        return updates;
    });
}

void Pd6710Controller::OnReadyIreqLow(PcmciaSlot&) {
    SetReadyIreqLow(true);
}

void Pd6710Controller::OnReadyIreqHigh(PcmciaSlot&) {
    SetReadyIreqLow(false);
}

void Pd6710Controller::SetReadyIreqLow(bool low) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    exca_.SetReadyIreqLow(low);
    UpdateManagementLineLocked();
    DriveCardLineLocked(false);
}

void Pd6710Controller::SaveState(StateWriter& w) {
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        w.Write("index", index_);
        w.Write("reg_chip_info", reg_chip_info_);
        w.WriteBytes("timing", timing_.data(), timing_.size());
        w.Write("misc_ctrl1", misc_ctrl1_);
        w.Write("fifo_scratch", fifo_scratch_);
        w.Write("misc_ctrl2", misc_ctrl2_);
        w.Write("ata_control", ata_control_);
        w.Write("scratchpad", scratchpad_);
        exca_.SaveState(w);
    }
    slot_.SaveSlotState(w);
}

void Pd6710Controller::RestoreState(StateReader& r) {
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        r.Read("index", index_);
        r.Read("reg_chip_info", reg_chip_info_);
        r.ReadBytes("timing", timing_.data(), timing_.size());
        r.Read("misc_ctrl1", misc_ctrl1_);
        r.Read("fifo_scratch", fifo_scratch_);
        r.Read("misc_ctrl2", misc_ctrl2_);
        r.Read("ata_control", ata_control_);
        r.Read("scratchpad", scratchpad_);
        exca_.RestoreState(r);
        card_request_ = exca_.FunctionalInterrupt() &&
                        exca_.CardIrqSelect() == emu_.Get<Pd6710CardIrqLine>().WiredIrqSelect();
    }
    slot_.RestoreSlotState(r);
}

void Pd6710Controller::PostRestore() {
    sequencer_.Run([this] {
        std::lock_guard<std::mutex> lk(state_mutex_);
        RouteManagementLocked(&intr_bits_, &irq_pin_bits_);
        emu_.Get<Pd6710ManagementIrqLine>().SetLevel(!PulseManagementLocked() && intr_bits_ != 0u);
        return ExcaSocketUpdates{{ {&slot_, ExcaWriteEffect::Outputs(exca_.OutputsEnabled())} }};
    });
    slot_.PostRestoreSlot();
    std::lock_guard<std::mutex> lk(state_mutex_);
    DriveCardLineLocked(true);
}

REGISTER_SERVICE(Pd6710Controller);

}

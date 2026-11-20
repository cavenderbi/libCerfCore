#include "intel_82365sl.h"

#include <cerfcore/boards/board_context.h>
#include "../../boards/nec_mobilepro_200/nec_mobilepro_200_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

#include <mutex>
#include <optional>

namespace cerfcore {

namespace {

constexpr char kSlotId[]    = "pc_card_slot";
constexpr char kSlotLabel[] = "PCMCIA #1";

constexpr uint8_t kRegIdentification   = 0x00u;
constexpr uint8_t kIdentificationRev0  = 0x82u;
constexpr uint8_t kRegReservedFirst    = 0x36u;
constexpr uint8_t kRegReservedLast     = 0x3Fu;
constexpr uint8_t kRegGuestClearedLast = 0x39u;

constexpr uint8_t kValidIndexEnd = 0x80u;
constexpr uint8_t kIntrCscToIntr = 0x10u;
constexpr uint8_t kNoIrqSelect   = 0x00u;

constexpr uint8_t  kWinMemcs16FullDecode = 0x20u;
constexpr unsigned kMemcs16UpperDecode   = 17u;
constexpr unsigned kMemcs16FullDecode    = 12u;

constexpr ExcaPartRules kExcaRules = [] {
    ExcaPartRules rules{};
    rules.ready_change_any_edge = false;
    rules.latch_requires_enable = false;
    rules.status_read_masked_by_enable = false;
    rules.auto_power = true;
    rules.power_control_reserved = 0x00u;
    rules.power_reserved_field_a = 0x00u;
    rules.power_reserved_code_a = 0x00u;
    rules.power_reserved_field_b = 0x00u;
    rules.power_reserved_code_b = 0x00u;
    rules.window_enable_reserved = 0x00u;
    rules.memory_end_high_reserved = 0x30u;
    rules.io_offset_low_reserved = 0x00u;
    rules.window_page_registers = false;
    rules.socket_control_power = false;
    rules.stschg_change_any_edge = false;
    rules.battery_dead_pin = ExcaBatteryDeadPin::kBvd1Low;
    rules.battery_warning = ExcaBatteryWarning::kEnterBvd1HighBvd2Low;
    rules.outputs_require_card = true;
    rules.outputs_require_vcc = true;
    rules.ring_indicate_masks_status_change = true;
    rules.irq_select_reserved = 0x2146u;
    rules.io_offset_registers = false;
    rules.memory_address_high_mask = 0x0Fu;
    rules.interrupt_control_reserved = 0x00u;
    rules.interrupt_control_reset = 0x00u;
    rules.removal_resets_interrupt_control = 0x00u;
    rules.removal_resets_power_control = false;
    rules.removal_resets_window_enable = false;
    rules.vcc_requires_card = false;
    rules.io_control_reserved = 0x00u;
    rules.reserved_bits_halt = true;
    rules.memory_start_high_reserved = 0x30u;
    rules.status_change_reserved = 0xF0u;
    return rules;
}();

}

Intel82365sl::Intel82365sl(CerfEmulator& emu)
    : IsaExcaSocketPair(emu, kExcaRules) {}

bool Intel82365sl::FittedOnFamily(std::string_view family) {
    return family == BoardFamilyId::NecMobilepro200;
}

bool Intel82365sl::ShouldRegister() {
    return FittedOnFamily(emu_.Get<BoardContext>().GetBoardFamilyId());
}

void Intel82365sl::OnReady() {
    board_ = &emu_.Get<Intel82365slBoard>();
    for (int b = 0; b < kSockets; ++b) Exca(b).SetCardInputsHighWithoutVcc(!board_->CardInputPullupsSwitched());
    StartSockets({kSlotId, kSlotLabel}, std::nullopt);
}

uint8_t Intel82365sl::UnbankedIndexReadLocked(uint32_t port) {
    return Floating().Read8("82365SL index read without a valid index", port);
}

uint8_t Intel82365sl::UnbankedDataReadLocked(uint32_t port) {
    return Floating().Read8("82365SL data read without a valid index", port);
}

bool Intel82365sl::SelectedLocked() const {
    if (reset_drive_)
        emu_.Get<Fatal>().Die("[82365SL] register access with INDEX 0x%02X while RESETDRV is high", IndexLocked());
    return index_valid_;
}

void Intel82365sl::IndexWrittenLocked(uint8_t index) {
    if (reset_drive_)
        emu_.Get<Fatal>().Die("[82365SL] index write 0x%02X while RESETDRV is high", index);
    index_valid_ = index < kValidIndexEnd;
}

bool Intel82365sl::ResetLineResets(ResetLineKind kind) {
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        reset_drive_ = false;
    }
    return board_->ResetDriveAtReset(kind);
}

void Intel82365sl::DriveResetDrive(bool high) {
    bool rising = false;
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        rising       = high && !reset_drive_;
        reset_drive_ = high;
    }
    if (rising) ResetController();
}

void Intel82365sl::SaveChipGlobal(StateWriter& w) const {
    w.Write<uint8_t>("index_valid", index_valid_ ? 1u : 0u);
    w.Write<uint8_t>("reset_drive", reset_drive_ ? 1u : 0u);
}

void Intel82365sl::RestoreChipGlobal(StateReader& r) {
    uint8_t index_valid = 0u;
    uint8_t reset_drive = 0u;
    r.Read("index_valid", index_valid);
    r.Read("reset_drive", reset_drive);
    index_valid_ = index_valid != 0u;
    reset_drive_ = reset_drive != 0u;
}

bool Intel82365sl::ChipOwns(uint8_t sub) const {
    return sub == kRegIdentification || (sub >= kRegReservedFirst && sub <= kRegReservedLast);
}

bool Intel82365sl::ChipReadLocked(int, uint8_t sub, uint8_t* value) {
    if (!ChipOwns(sub)) return false;
    if (sub == kRegIdentification) {
        *value = kIdentificationRev0;
        return true;
    }
    emu_.Get<Fatal>().Die("[82365SL] read of reserved INDEX 0x%02X", IndexLocked());
}

void Intel82365sl::ChipWriteLocked(int, uint8_t sub, uint8_t value, ExcaWriteEffect*) {
    if (sub >= kRegReservedFirst && sub <= kRegGuestClearedLast && value == 0u) return;
    emu_.Get<Fatal>().Die("[82365SL] write 0x%02X to identification or reserved INDEX 0x%02X", value,
                          IndexLocked());
}

bool Intel82365sl::Memcs16AssertedLocked(uint32_t off) {
    if (!board_->HostSamplesMemcs16()) return false;
    for (int b = 0; b < kSockets; ++b) {
        const unsigned shift = (Exca(b).WindowEnable() & kWinMemcs16FullDecode) != 0u
            ? kMemcs16FullDecode : kMemcs16UpperDecode;
        if (Exca(b).SixteenBitWindowDecodes(off, shift)) return true;
    }
    return false;
}

void Intel82365sl::UpdateIrqLineLocked() {
    const uint8_t wired = board_->WiredIrqSelect();
    bool pending = false;
    for (int b = 0; b < kSockets; ++b) {
        Pcic82365& exca = Exca(b);
        const bool csc_active = exca.StatusChangeInterruptActive();
        if ((exca.InterruptAndGeneralControl() & kIntrCscToIntr) != 0u) {
            if (csc_active)
                emu_.Get<Fatal>().Die("[82365SL] socket %d status change interrupt on INTR#, whose board "
                                      "connection is not modeled", b);
        } else if (exca.CscIrqSelect() == wired) {
            pending = pending || csc_active;
        } else if (csc_active && exca.CscIrqSelect() != kNoIrqSelect) {
            emu_.Get<Fatal>().Die("[82365SL] socket %d status change interrupt on IRQ%u, whose board "
                                  "connection is not modeled", b, exca.CscIrqSelect());
        }
        if (!exca.IoCardInterface()) continue;
        const bool card_active = exca.IreqPinLow();
        if (exca.CardIrqSelect() == wired) {
            pending = pending || card_active;
        } else if (card_active && exca.CardIrqSelect() != kNoIrqSelect) {
            emu_.Get<Fatal>().Die("[82365SL] socket %d card interrupt on IRQ%u, whose board connection is "
                                  "not modeled", b, exca.CardIrqSelect());
        }
    }
    board_->SetIrqLevel(pending);
}

REGISTER_SERVICE(Intel82365sl);

}

#include "casio_cassiopeia_a10_cf_slot.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../core/board_parts.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "casio_cassiopeia_a10_chip_intc.h"
#include "casio_cassiopeia_a10_compact_flash_part_id.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_pin_registers.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10CfSlot);

namespace {

constexpr char kSlotId[]    = "cf_slot";
constexpr char kSlotLabel[] = "CF slot";

constexpr uint32_t kControl = 0x10000280u;
constexpr uint32_t kStatus  = 0x10000282u;

constexpr uint16_t kSocketOn    = 0x0003u;
constexpr uint16_t kRegLevel    = 0x0004u;
constexpr uint16_t kCardReset   = 0x0008u;
constexpr uint16_t kControlBits = kSocketOn | kRegLevel | kCardReset;
constexpr uint16_t kCardAbsent  = 0x0003u;
constexpr uint16_t kReady       = 0x0008u;

}

CasioCassiopeiaA10CfSlot::CasioCassiopeiaA10CfSlot(CerfEmulator& emu)
    : Peripheral(emu), slot_(emu, *this, {kSlotId, kSlotLabel}) {}

bool CasioCassiopeiaA10CfSlot::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
           emu_.Get<BoardParts>().Has(PartId::CasioCassiopeiaA10CompactFlash);
}

void CasioCassiopeiaA10CfSlot::OnReady() {
    chip_intc_ = &emu_.Get<CasioCassiopeiaA10ChipIntc>();
    pins_      = &emu_.Get<CasioCassiopeiaA10PinRegisters>();
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<MachineObjectTree>().Add(&slot_);
    GuestCpuReset& reset = emu_.Get<GuestCpuReset>();
    reset.RegisterResetListener([this](ResetLineKind) { Reset(); });
    reset.RegisterResetReleaseListener([this] { ApplyControl(0u); });
    DriveIreqLine();
}

void CasioCassiopeiaA10CfSlot::OnShutdown() { slot_.OnShutdown(); }

uint32_t CasioCassiopeiaA10CfSlot::MmioBase() const { return kControl; }
uint32_t CasioCassiopeiaA10CfSlot::MmioSize() const { return kStatus + 2u - kControl; }

uint16_t CasioCassiopeiaA10CfSlot::ReadHalf(uint32_t addr) {
    if (addr == kStatus) {
        std::lock_guard<std::mutex> lk(mtx_);
        const uint16_t              status = Status();
        card_change_                       = false;
        DriveCardChangeLocked();
        return status;
    }
    if (addr != kControl) HaltUnsupportedAccess("ReadHalf", addr, 0u);
    std::lock_guard<std::mutex> lk(mtx_);
    return control_;
}

void CasioCassiopeiaA10CfSlot::WriteHalf(uint32_t addr, uint16_t value) {
    const uint16_t socket = static_cast<uint16_t>(value & kSocketOn);
    if (addr != kControl || (value & ~kControlBits) != 0u || (socket != 0u && socket != kSocketOn)) {
        HaltUnsupportedAccess("WriteHalf", addr, value);
    }
    {
        std::lock_guard<std::mutex> lk(mtx_);
        control_ = value;
    }
    ApplyControl(value);
}

void CasioCassiopeiaA10CfSlot::SaveState(StateWriter& w) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        w.Write("control", control_);
        w.Write("card_change", card_change_);
    }
    status_.SaveState(w);
    slot_.SaveSlotState(w);
}

void CasioCassiopeiaA10CfSlot::RestoreState(StateReader& r) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        r.Read("control", control_);
        r.Read("card_change", card_change_);
    }
    status_.RestoreState(r);
    slot_.RestoreSlotState(r);
}

void CasioCassiopeiaA10CfSlot::PostRestore() {
    slot_.PostRestoreSlot();
    DriveIreqLine();
    std::lock_guard<std::mutex> lk(mtx_);
    DriveCardChangeLocked();
}

void CasioCassiopeiaA10CfSlot::OnCardDetectChanged(PcmciaSlot&) {
    std::lock_guard<std::mutex> lk(mtx_);
    card_change_ = true;
    DriveCardChangeLocked();
}

void CasioCassiopeiaA10CfSlot::OnReadyIreqLow(PcmciaSlot&) {
    status_.SetReadyIreqLow(true);
    DriveIreqLine();
}

void CasioCassiopeiaA10CfSlot::OnReadyIreqHigh(PcmciaSlot&) {
    status_.SetReadyIreqLow(false);
    DriveIreqLine();
}

PcmciaSlot& CasioCassiopeiaA10CfSlot::Slot() { return slot_; }

bool CasioCassiopeiaA10CfSlot::AttributeSelected() {
    std::lock_guard<std::mutex> lk(mtx_);
    return (control_ & kRegLevel) == 0u;
}

uint16_t CasioCassiopeiaA10CfSlot::Status() const {
    uint16_t status = slot_.HasCard() ? 0u : kCardAbsent;
    if (slot_.IsPowered() && !status_.ReadyIreqLow()) status = static_cast<uint16_t>(status | kReady);
    return status;
}

void CasioCassiopeiaA10CfSlot::ApplyControl(uint16_t control) {
    const bool on = (control & kSocketOn) == kSocketOn;
    slot_.SetPowered(on);
    slot_.SetCardEnabled(on);
    slot_.SetResetLine((control & kCardReset) != 0u);
}

void CasioCassiopeiaA10CfSlot::DriveIreqLine() {
    pins_->SetLine(kCasioCassiopeiaA10Pin05E, !status_.ReadyIreqLow());
}

void CasioCassiopeiaA10CfSlot::DriveCardChangeLocked() {
    chip_intc_->SetInputLevel(CasioCassiopeiaA10ChipInput::kCfCardChange, card_change_);
}

void CasioCassiopeiaA10CfSlot::Reset() {
    std::lock_guard<std::mutex> lk(mtx_);
    control_     = 0u;
    card_change_ = false;
    DriveCardChangeLocked();
}

}

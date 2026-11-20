#include "casio_cassiopeia_a10_pc_card.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/pcmcia/pcmcia_auto_insert.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "casio_cassiopeia_a10_card_wiring.h"
#include "casio_cassiopeia_a10_family_id.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10PcCard);

namespace {

constexpr char kSlotId[]    = "pc_card_slot";
constexpr char kSlotLabel[] = "PC Card slot";

constexpr uint32_t kEvents = 0x10000320u;
constexpr uint32_t kPower  = 0x10000322u;
constexpr uint32_t kReset  = 0x10000324u;
constexpr uint32_t kStatus = 0x10000326u;
constexpr uint32_t kVcc    = 0x1000032Au;
constexpr uint32_t kSpeakerRoute = 0x1000032Cu;

constexpr uint16_t kIreqEvent          = 0x0001u;
constexpr uint16_t kStatusChangeEvent  = 0x0002u;
constexpr uint16_t kEventBits          = 0x0003u;
constexpr uint16_t kIreqEnable         = 0x0004u;
constexpr uint16_t kStatusChangeEnable = 0x0008u;
constexpr uint16_t kDetectBits         = 0x00C0u;
constexpr uint16_t kDetectShift        = 6u;
constexpr uint16_t kDetectPin1Enable   = 0x0100u;
constexpr uint16_t kDetectPin2Enable   = 0x0200u;
constexpr uint16_t kEnableBits         = 0x030Cu;
constexpr uint16_t kUpperBits          = 0xF000u;
constexpr uint16_t kDetectPin1         = 0x0001u;
constexpr uint16_t kDetectPin2         = 0x0002u;
constexpr uint16_t kBothDetectPins     = 0x0003u;

constexpr uint16_t kCardEnable = 0x0001u;
constexpr uint16_t kPowerOn    = 0x0002u;
constexpr uint16_t kPowerBits  = 0x0007u;
constexpr uint16_t kCardReset  = 0x0001u;
constexpr uint16_t kBvd2       = 0x0010u;
constexpr uint16_t kVs1        = 0x0100u;
constexpr uint16_t kVccOn      = 0x0002u;
constexpr uint16_t kVccWritableBits = 0x000Fu;
constexpr uint16_t kSpeakerOn  = 0x0001u;

}

CasioCassiopeiaA10PcCard::CasioCassiopeiaA10PcCard(CerfEmulator& emu)
    : Peripheral(emu), slot_(emu, *this, {kSlotId, kSlotLabel}) {}

bool CasioCassiopeiaA10PcCard::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
}

void CasioCassiopeiaA10PcCard::OnReady() {
    wiring_ = &emu_.Get<CasioCassiopeiaA10CardWiring>();
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<MachineObjectTree>().Add(&slot_);
    emu_.Get<PcmciaAutoInsert>().InsertLaunchCompactFlash(slot_);
    GuestCpuReset& reset = emu_.Get<GuestCpuReset>();
    reset.RegisterResetListener([this](ResetLineKind) { Reset(); });
    reset.RegisterResetReleaseListener([this] {
        slot_.SetPowered(false);
        slot_.SetCardEnabled(false);
        slot_.SetResetLine(false);
    });
}

void CasioCassiopeiaA10PcCard::OnShutdown() { slot_.OnShutdown(); }

uint32_t CasioCassiopeiaA10PcCard::MmioBase() const { return kEvents; }
uint32_t CasioCassiopeiaA10PcCard::MmioSize() const { return kSpeakerRoute + 2u - kEvents; }

uint16_t CasioCassiopeiaA10PcCard::ReadHalf(uint32_t addr) {
    switch (addr) {
    case kEvents: {
        std::lock_guard<std::mutex> lk(mtx_);
        return static_cast<uint16_t>(enables_ | upper_ | latches_ | (detect_ << kDetectShift));
    }
    case kPower: {
        std::lock_guard<std::mutex> lk(mtx_);
        return power_;
    }
    case kReset: {
        std::lock_guard<std::mutex> lk(mtx_);
        return reset_;
    }
    case kStatus:
        return static_cast<uint16_t>(status_.Word(slot_) | kBvd2 | (slot_.Vs1Grounded() ? 0u : kVs1));
    case kVcc: {
        std::lock_guard<std::mutex> lk(mtx_);
        return vcc_;
    }
    default:
        HaltUnsupportedAccess("ReadHalf", addr, 0u);
    }
}

void CasioCassiopeiaA10PcCard::WriteHalf(uint32_t addr, uint16_t value) {
    switch (addr) {
    case kEvents:
        WriteEvents(addr, value);
        return;
    case kPower:
        if ((value & ~kPowerBits) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
        {
            std::lock_guard<std::mutex> lk(mtx_);
            power_ = value;
        }
        ApplyPower();
        return;
    case kReset:
        if ((value & ~kCardReset) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
        {
            std::lock_guard<std::mutex> lk(mtx_);
            reset_ = value;
        }
        slot_.SetResetLine((value & kCardReset) != 0u);
        return;
    case kVcc:
        if ((value & ~kVccWritableBits) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
        {
            std::lock_guard<std::mutex> lk(mtx_);
            vcc_ = value;
        }
        ApplyPower();
        return;
    case kSpeakerRoute: {
        if ((value & ~kSpeakerOn) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
        std::lock_guard<std::mutex> lk(mtx_);
        speaker_route_ = value;
        return;
    }
    default:
        HaltUnsupportedAccess("WriteHalf", addr, value);
    }
}

void CasioCassiopeiaA10PcCard::WriteEvents(uint32_t addr, uint16_t value) {
    if ((value & ~(kEnableBits | kUpperBits | kEventBits | kDetectBits)) != 0u) {
        HaltUnsupportedAccess("WriteHalf", addr, value);
    }
    std::lock_guard<std::mutex> lk(mtx_);
    const uint16_t rising = static_cast<uint16_t>(value & ~enables_ & kEnableBits);
    upper_                = static_cast<uint16_t>(value & kUpperBits);
    latches_ = static_cast<uint16_t>(latches_ & ~(value & kEventBits));
    detect_  = static_cast<uint16_t>(detect_ & ~((value & kDetectBits) >> kDetectShift));
    enables_ = static_cast<uint16_t>(value & kEnableBits);
    if ((rising & kIreqEnable) != 0u && status_.ReadyIreqLow()) latches_ = static_cast<uint16_t>(latches_ | kIreqEvent);
    if ((rising & kStatusChangeEnable) != 0u && slot_.StatusChangeLow()) {
        latches_ = static_cast<uint16_t>(latches_ | kStatusChangeEvent);
    }
    DriveLinesLocked();
}

void CasioCassiopeiaA10PcCard::SaveState(StateWriter& w) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        w.Write("enables", enables_);
        w.Write("upper", upper_);
        w.Write("latches", latches_);
        w.Write("detect", detect_);
        w.Write("power", power_);
        w.Write("reset", reset_);
        w.Write("vcc", vcc_);
        w.Write("speaker_route", speaker_route_);
    }
    status_.SaveState(w);
    slot_.SaveSlotState(w);
}

void CasioCassiopeiaA10PcCard::RestoreState(StateReader& r) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        r.Read("enables", enables_);
        r.Read("upper", upper_);
        r.Read("latches", latches_);
        r.Read("detect", detect_);
        r.Read("power", power_);
        r.Read("reset", reset_);
        r.Read("vcc", vcc_);
        r.Read("speaker_route", speaker_route_);
    }
    status_.RestoreState(r);
    slot_.RestoreSlotState(r);
}

void CasioCassiopeiaA10PcCard::PostRestore() {
    slot_.PostRestoreSlot();
    std::lock_guard<std::mutex> lk(mtx_);
    DriveLinesLocked();
}

void CasioCassiopeiaA10PcCard::OnCardDetectChanged(PcmciaSlot&) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        detect_ = kBothDetectPins;
        DriveLinesLocked();
    }
    detect_listeners_.Call();
}

void CasioCassiopeiaA10PcCard::AddCardDetectListener(CallbackList<>::Callback listener) {
    detect_listeners_.Add(std::move(listener));
}

void CasioCassiopeiaA10PcCard::OnReadyIreqLow(PcmciaSlot&) {
    status_.SetReadyIreqLow(true);
    std::lock_guard<std::mutex> lk(mtx_);
    if ((enables_ & kIreqEnable) != 0u) latches_ = static_cast<uint16_t>(latches_ | kIreqEvent);
    DriveLinesLocked();
}

void CasioCassiopeiaA10PcCard::OnStatusChange(PcmciaSlot&, bool low) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (low && (enables_ & kStatusChangeEnable) != 0u) latches_ = static_cast<uint16_t>(latches_ | kStatusChangeEvent);
    DriveLinesLocked();
}

void CasioCassiopeiaA10PcCard::OnReadyIreqHigh(PcmciaSlot&) {
    status_.SetReadyIreqLow(false);
    std::lock_guard<std::mutex> lk(mtx_);
    DriveLinesLocked();
}

PcmciaSlot& CasioCassiopeiaA10PcCard::Slot() { return slot_; }

bool CasioCassiopeiaA10PcCard::SocketVccOn() {
    std::lock_guard<std::mutex> lk(mtx_);
    return (vcc_ & kVccOn) != 0u;
}

void CasioCassiopeiaA10PcCard::ApplyPower() {
    uint16_t power = 0u;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        power = power_;
    }
    slot_.SetPowered((power & kPowerOn) != 0u && wiring_->SupplyOn());
    slot_.SetCardEnabled((power & kCardEnable) != 0u);
}

void CasioCassiopeiaA10PcCard::DriveLinesLocked() {
    const bool status_change = (latches_ & kStatusChangeEvent) != 0u && (enables_ & kStatusChangeEnable) != 0u;
    const bool pin1_change   = (detect_ & kDetectPin1) != 0u && (enables_ & kDetectPin1Enable) != 0u;
    const bool pin2_change   = (detect_ & kDetectPin2) != 0u && (enables_ & kDetectPin2Enable) != 0u;
    const CasioCassiopeiaA10CardWiring::Lines lines{
        .change       = status_change || pin1_change || pin2_change,
        .ireq_latch   = (latches_ & kIreqEvent) != 0u && (enables_ & kIreqEnable) != 0u,
        .ireq_low     = status_.ReadyIreqLow(),
        .card_present = slot_.HasCard(),
    };
    wiring_->Drive(lines);
}

void CasioCassiopeiaA10PcCard::Reset() {
    std::lock_guard<std::mutex> lk(mtx_);
    enables_ = 0u;
    upper_   = 0u;
    latches_ = 0u;
    detect_  = 0u;
    power_   = 0u;
    reset_   = 0u;
    vcc_           = 0u;
    speaker_route_ = 0u;
    DriveLinesLocked();
}

}

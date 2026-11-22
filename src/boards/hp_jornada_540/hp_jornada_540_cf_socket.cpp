#include "hp_jornada_540_cf_socket.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/state/hibernation.h>
#include <cerfcore/state/state_stream.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

REGISTER_SERVICE(HpJornada540CfSocket);

namespace {

constexpr char kSlotId[]    = "cf_slot";
constexpr char kSlotLabel[] = "CF slot";

constexpr Sh77xxIntcPin kCardDetectPin   = Sh77xxPintPin(14u);
constexpr Sh77xxIntcPin kStatusChangePin = Sh77xxPintPin(15u);
constexpr Sh77xxIntcPin kReadyPin        = Sh77xxIntcPin::kIrq0;

}

HpJornada540CfSocket::HpJornada540CfSocket(CerfEmulator& emu)
    : Service(emu), slot_(emu, *this, {kSlotId, kSlotLabel}) {}

bool HpJornada540CfSocket::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

void HpJornada540CfSocket::OnReady() {
    intc_ = &emu_.Get<Sh77xxIntc>();
    emu_.Get<MachineObjectTree>().Add(&slot_);
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] {
        slot_.PostRestoreSlot();
        DriveLines();
    });
    DriveLines();
}

void HpJornada540CfSocket::OnShutdown() { slot_.OnShutdown(); }

void HpJornada540CfSocket::SetCardPower(bool on) { slot_.SetPowered(on); }

void HpJornada540CfSocket::SetResetLine(bool asserted) { slot_.SetResetLine(asserted); }

void HpJornada540CfSocket::SaveState(StateWriter& w) {
    w.Write("cf_ready_ireq_low", ready_ireq_low_.load(std::memory_order_acquire));
    slot_.SaveSlotState(w);
}

void HpJornada540CfSocket::RestoreState(StateReader& r) {
    bool low = false;
    r.Read("cf_ready_ireq_low", low);
    ready_ireq_low_.store(low, std::memory_order_release);
    slot_.RestoreSlotState(r);
}

void HpJornada540CfSocket::OnCardDetectChanged(PcmciaSlot&) { intc_->SetPinLevel(kCardDetectPin, CardAbsent()); }

void HpJornada540CfSocket::OnReadyIreqLow(PcmciaSlot&) {
    ready_ireq_low_.store(true, std::memory_order_release);
    intc_->SetPinLevel(kReadyPin, false);
}

void HpJornada540CfSocket::OnReadyIreqHigh(PcmciaSlot&) {
    ready_ireq_low_.store(false, std::memory_order_release);
    intc_->SetPinLevel(kReadyPin, true);
}

void HpJornada540CfSocket::OnStatusChange(PcmciaSlot&, bool low) { intc_->SetPinLevel(kStatusChangePin, !low); }

void HpJornada540CfSocket::DriveLines() {
    intc_->SetPinLevel(kCardDetectPin, CardAbsent());
    intc_->SetPinLevel(kStatusChangePin, !slot_.StatusChangeLow());
    intc_->SetPinLevel(kReadyPin, !ready_ireq_low_.load(std::memory_order_acquire));
}

}

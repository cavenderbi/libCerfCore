#include "pci1225_interrupt_pin.h"

#include "ti_cardbus_board.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

void Pci1225InterruptPin::Attach(TiCardbusBoard& board, const Pci1225InterruptPin& peer) {
    board_ = &board;
    peer_ = &peer;
}

void Pci1225InterruptPin::Drive(bool active, uint8_t slot, bool serialized, bool force) {
    const bool same_output = slot == slot_ && serialized == serialized_;
    if (!force && active == asserted_ && same_output)
        return;
    if (!same_output && asserted_)
        board_->SetPciInterruptLevel(slot_, serialized_, peer_->DrivesSlot(slot_, serialized_));
    asserted_ = active;
    slot_ = slot;
    serialized_ = serialized;
    board_->SetPciInterruptLevel(slot, serialized, active || peer_->DrivesSlot(slot, serialized));
}

void Pci1225InterruptPin::SaveState(StateWriter& w) const {
    w.Write<uint32_t>("irq_asserted", asserted_ ? 1u : 0u);
    w.Write("signaled_slot", slot_);
    w.Write<uint8_t>("signaled_serialized", serialized_ ? 1u : 0u);
}

void Pci1225InterruptPin::RestoreState(StateReader& r) {
    uint32_t asserted = 0u;
    r.Read("irq_asserted", asserted);
    asserted_ = asserted != 0u;
    r.Read("signaled_slot", slot_);
    uint8_t serialized = 0u;
    r.Read("signaled_serialized", serialized);
    serialized_ = serialized != 0u;
}

}

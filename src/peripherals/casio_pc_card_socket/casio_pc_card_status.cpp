#include "casio_pc_card_status.h"

#include <cerfcore/state/state_stream.h>
#include "../pcmcia/pcmcia_slot.h"

namespace cerfcore {

void CasioPcCardStatus::SetReadyIreqLow(bool low) { ready_ireq_low_.store(low, std::memory_order_release); }

bool CasioPcCardStatus::ReadyIreqLow() const { return ready_ireq_low_.load(std::memory_order_acquire); }

uint16_t CasioPcCardStatus::Word(const PcmciaSlot& slot) const {
    uint16_t v = slot.StatusChangeLow() ? 0u : kBvd1;
    if (!slot.HasCard()) v = static_cast<uint16_t>(v | kCardAbsent);
    if (slot.IsPowered() && !ReadyIreqLow()) v = static_cast<uint16_t>(v | kReady);
    return v;
}

void CasioPcCardStatus::SaveState(StateWriter& w) const {
    w.Write<uint8_t>("ready_ireq_low", ReadyIreqLow() ? 1u : 0u);
}

void CasioPcCardStatus::RestoreState(StateReader& r) {
    uint8_t ready_ireq_low = 0u;
    r.Read("ready_ireq_low", ready_ireq_low);
    SetReadyIreqLow(ready_ireq_low != 0u);
}

}

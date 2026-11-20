#include "casio_cassiopeia_a10_slot_inputs.h"

#include "casio_cassiopeia_a10_battery.h"
#include "casio_cassiopeia_a10_pc_card.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10SlotInputs);

void CasioCassiopeiaA10SlotInputs::OnReady() {
    battery_ = &emu_.Get<CasioCassiopeiaA10Battery>();
    pc_card_ = &emu_.Get<CasioCassiopeiaA10PcCard>();
    CasioCassiopeiaA10ChipWord::OnReady();
}

uint16_t CasioCassiopeiaA10SlotInputs::ReadHalf(uint32_t) { return Inputs(); }

uint16_t CasioCassiopeiaA10SlotInputs::Inputs() const {
    uint16_t inputs = pc_card_->Slot().HasCard() ? 0u : kCasioCassiopeiaA10CardReleased;
    if (battery_->OnExternalPower()) inputs = static_cast<uint16_t>(inputs | kCasioCassiopeiaA10AdapterInUse);
    return inputs;
}

}

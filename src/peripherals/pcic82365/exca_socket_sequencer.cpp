#include "exca_socket_sequencer.h"

#include "../pcmcia/pcmcia_slot.h"

namespace cerfcore {

void ExcaSocketSequencer::Apply(PcmciaSlot& slot, const ExcaWriteEffect& effect) {
    if (effect.power_changed)   slot.SetPowered(effect.power_on);
    if (effect.outputs_changed) slot.SetCardEnabled(effect.outputs_on);
    if (effect.reset_changed)   slot.SetResetLine(effect.reset_asserted);
}

}

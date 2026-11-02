#pragma once

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;
class TiCardbusBoard;

class Pci1225InterruptPin {
public:
    void Attach(TiCardbusBoard& board, const Pci1225InterruptPin& peer);
    void Drive(bool active, uint8_t slot, bool serialized, bool force);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    bool DrivesSlot(uint8_t slot, bool serialized) const {
        return asserted_ && slot_ == slot && serialized_ == serialized;
    }

    TiCardbusBoard*            board_      = nullptr;
    const Pci1225InterruptPin* peer_       = nullptr;
    bool                       asserted_   = false;
    uint8_t                    slot_       = 0u;
    bool                       serialized_ = false;
};

}

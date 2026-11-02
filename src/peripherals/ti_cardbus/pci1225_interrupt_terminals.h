#pragma once

#include <cstdint>

namespace cerfcore {

struct Pci1225SharedConfig;

class Pci1225InterruptTerminals {
public:
    Pci1225InterruptTerminals(uint8_t function, const Pci1225SharedConfig& shared)
        : function_(function), shared_(shared) {}

    static bool     RoutingModeled(uint32_t mfunc);
    static uint16_t IsaIrqBit(uint8_t select);

    uint8_t  InterruptPin() const;
    uint8_t  SignaledSlot() const;
    bool     PciInterruptSignaled() const;
    bool     PciInterruptsSerialized() const;
    bool     IsaIrqsSerialized() const;
    uint16_t SignaledIsaIrqs(uint16_t requested) const;
    uint16_t DrivenParallelIsaIrqs() const;

private:
    uint8_t                    function_;
    const Pci1225SharedConfig& shared_;
};

}

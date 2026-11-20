#pragma once

#include "../pcmcia/pcmcia_slot.h"

#include <cstdint>

namespace cerfcore {

struct HitachiPccSocketPins {
    bool ready;
    bool write_protect;
    bool vs2;
    bool vs1;
    bool cd2;
    bool cd1;
    bool bvd2;
    bool bvd1;
    bool wait;
};

struct HitachiPccCardCycle {
    PcmciaSlot* slot;
    uint32_t    card_address;
    bool        attribute;
    bool        dropped;
};

class HitachiPcc : public PcmciaSlotHost {
public:
    static constexpr uint32_t kCauseBatteryDead    = 0x01u;
    static constexpr uint32_t kCauseBatteryWarning = 0x02u;
    static constexpr uint32_t kCauseReadyChange    = 0x04u;
    static constexpr uint32_t kCauseDetectChange   = 0x08u;
    static constexpr uint32_t kCauseStatusChange   = 0x10u;
    static constexpr uint32_t kCauseIreq           = 0x20u;
    static constexpr uint32_t kCauseSoftwareDetect = 0x40u;

    virtual HitachiPccCardCycle DecodeCycle(uint32_t area, uint32_t offset, bool io, bool write) = 0;
};

}

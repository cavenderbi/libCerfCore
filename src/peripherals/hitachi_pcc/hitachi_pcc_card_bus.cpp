#include "hitachi_pcc_card_bus.h"

#include "../pcmcia/pcmcia_host_cycles.h"

namespace cerfcore {

namespace {

constexpr uint32_t kFirstPccArea = 5u;
constexpr uint32_t kLastPccArea  = 6u;

}

bool HitachiPccCardBus::ServesArea(uint32_t area) const { return area >= kFirstPccArea && area <= kLastPccArea; }

uint32_t HitachiPccCardBus::ReadCycles(uint32_t area, uint32_t offset, bool io, PcmciaIoWidth width, unsigned bytes) {
    const HitachiPccCardCycle c = Pcc().DecodeCycle(area, offset, io, false);
    if (io) return ReadCardIo(*c.slot, c.card_address, width, bytes);
    return ReadCardMemory(*c.slot, c.card_address, c.attribute, width != PcmciaIoWidth::kEightBit, bytes);
}

void HitachiPccCardBus::WriteCycles(uint32_t area, uint32_t offset, bool io, PcmciaIoWidth width, unsigned bytes,
                                    uint32_t value) {
    const HitachiPccCardCycle c = Pcc().DecodeCycle(area, offset, io, true);
    if (c.dropped) return;
    if (io) {
        WriteCardIo(*c.slot, c.card_address, width, bytes, value);
        return;
    }
    WriteCardMemory(*c.slot, c.card_address, c.attribute, width != PcmciaIoWidth::kEightBit, bytes, value);
}

}

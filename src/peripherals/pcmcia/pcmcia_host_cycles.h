#pragma once

#include "pcmcia_io_width.h"

#include <cstdint>

namespace cerfcore {

class PcmciaSlot;

unsigned MemoryCardCycles(bool sixteen_bit_path, unsigned bytes);
unsigned IoCardCycles(PcmciaSlot& slot, uint32_t offset, PcmciaIoWidth width, unsigned bytes);
uint32_t ReadCardMemory(PcmciaSlot& slot, uint32_t offset, bool attribute, bool sixteen_bit_path,
                        unsigned bytes);
void     WriteCardMemory(PcmciaSlot& slot, uint32_t offset, bool attribute, bool sixteen_bit_path,
                         unsigned bytes, uint32_t value);
bool     SixteenBitCardCycle(PcmciaSlot& slot, uint32_t offset, PcmciaIoWidth width);
uint32_t ReadCardIo(PcmciaSlot& slot, uint32_t offset, PcmciaIoWidth width, unsigned bytes);
void     WriteCardIo(PcmciaSlot& slot, uint32_t offset, PcmciaIoWidth width, unsigned bytes,
                     uint32_t value);

}

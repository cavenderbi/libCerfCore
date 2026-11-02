#include "pcmcia_host_cycles.h"

#include "pcmcia_slot.h"

namespace cerfcore {

unsigned MemoryCardCycles(bool sixteen_bit_path, unsigned bytes) {
    if (bytes == 1u) return 1u;
    return sixteen_bit_path ? bytes / 2u : bytes;
}

unsigned IoCardCycles(PcmciaSlot& slot, uint32_t offset, PcmciaIoWidth width, unsigned bytes) {
    if (bytes == 1u) return 1u;
    unsigned cycles = 0u;
    for (unsigned k = 0u; k < bytes; k += 2u)
        cycles += SixteenBitCardCycle(slot, offset + k, width) ? 1u : 2u;
    return cycles;
}

uint32_t ReadCardMemory(PcmciaSlot& slot, uint32_t offset, bool attribute, bool sixteen_bit_path,
                        unsigned bytes) {
    if (bytes == 1u) return attribute ? slot.ReadAttribute8(offset) : slot.ReadCommon8(offset);
    const unsigned step = sixteen_bit_path ? 2u : 1u;
    uint32_t value = 0u;
    for (unsigned k = 0u; k < bytes; k += step) {
        const uint32_t at = offset + k;
        const uint32_t part = step == 2u
            ? (attribute ? slot.ReadAttribute16(at) : slot.ReadCommon16(at))
            : (attribute ? slot.ReadAttribute8(at) : slot.ReadCommon8(at));
        value |= part << (8u * k);
    }
    return value;
}

void WriteCardMemory(PcmciaSlot& slot, uint32_t offset, bool attribute, bool sixteen_bit_path,
                     unsigned bytes, uint32_t value) {
    if (bytes == 1u) {
        if (attribute) slot.WriteAttribute8(offset, static_cast<uint8_t>(value));
        else           slot.WriteCommon8(offset, static_cast<uint8_t>(value));
        return;
    }
    const unsigned step = sixteen_bit_path ? 2u : 1u;
    for (unsigned k = 0u; k < bytes; k += step) {
        const uint32_t at = offset + k;
        const uint32_t part = value >> (8u * k);
        if (step == 2u) {
            if (attribute) slot.WriteAttribute16(at, static_cast<uint16_t>(part));
            else           slot.WriteCommon16(at, static_cast<uint16_t>(part));
        } else {
            if (attribute) slot.WriteAttribute8(at, static_cast<uint8_t>(part));
            else           slot.WriteCommon8(at, static_cast<uint8_t>(part));
        }
    }
}

bool SixteenBitCardCycle(PcmciaSlot& slot, uint32_t offset, PcmciaIoWidth width) {
    switch (width) {
        case PcmciaIoWidth::kSixteenBit:
            return true;
        case PcmciaIoWidth::kCardIois16:
            return slot.Iois16(offset);
        case PcmciaIoWidth::kEightBit:
            break;
    }
    return false;
}

uint32_t ReadCardIo(PcmciaSlot& slot, uint32_t offset, PcmciaIoWidth width, unsigned bytes) {
    if (bytes == 1u)
        return slot.ReadIo8(offset);
    uint32_t value = 0u;
    for (unsigned k = 0u; k < bytes; k += 2u) {
        const uint32_t at = offset + k;
        uint32_t part = 0u;
        if (SixteenBitCardCycle(slot, at, width)) {
            part = slot.ReadIo16(at);
        } else {
            part = slot.ReadIo8(at);
            part |= static_cast<uint32_t>(slot.ReadIo8(at + 1u)) << 8;
        }
        value |= part << (8u * k);
    }
    return value;
}

void WriteCardIo(PcmciaSlot& slot, uint32_t offset, PcmciaIoWidth width, unsigned bytes,
                 uint32_t value) {
    if (bytes == 1u) {
        slot.WriteIo8(offset, static_cast<uint8_t>(value));
        return;
    }
    for (unsigned k = 0u; k < bytes; k += 2u) {
        const uint32_t at = offset + k;
        const uint32_t part = value >> (8u * k);
        if (SixteenBitCardCycle(slot, at, width)) {
            slot.WriteIo16(at, static_cast<uint16_t>(part));
        } else {
            slot.WriteIo8(at, static_cast<uint8_t>(part));
            slot.WriteIo8(at + 1u, static_cast<uint8_t>(part >> 8));
        }
    }
}

}

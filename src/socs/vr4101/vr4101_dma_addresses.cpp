#include "vr4101_dma_addresses.h"

#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/cpu/emulated_memory.h>

namespace cerfcore {

namespace {

const char* ChannelName(Vr4101DmaChannel channel) {
    switch (channel) {
        case Vr4101DmaChannel::kPiu:   return "PIU";
        case Vr4101DmaChannel::kSiuRx: return "SIU RX";
        case Vr4101DmaChannel::kSiuTx: return "SIU TX";
        case Vr4101DmaChannel::kAiu:   return "AIU";
        case Vr4101DmaChannel::kKiu:   return "KIU";
    }
    return "?";
}

}

uint16_t Vr4101DmaAddresses::Load(Vr4101DmaChannel channel) {
    const uint32_t pa = Address(channel);
    uint8_t        raw[2];
    if (emu_.Get<EmulatedMemory>().TryCopyOut(pa, raw, 2u) != 2u) {
        emu_.Get<Fatal>().Die("VR4101 %s DMA reads unbacked PA 0x%08X", ChannelName(channel), pa);
    }
    return cerf::le::U16(raw);
}

bool Vr4101DmaAddresses::Store(Vr4101DmaChannel channel, uint16_t word) {
    const uint32_t pa = Address(channel);
    uint8_t        raw[2];
    cerf::le::Put16(raw, word);
    if (emu_.Get<EmulatedMemory>().TryCopyIn(pa, raw, 2u) != 2u) {
        emu_.Get<Fatal>().Die("VR4101 %s DMA writes unbacked PA 0x%08X", ChannelName(channel), pa);
    }
    return Step(channel);
}

}

#include "sh77xx_dma_address_map.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "sh77xx_adc.h"
#include "sh77xx_area1.h"
#include "sh77xx_dac.h"
#include "sh77xx_pfc.h"
#include "sh77xx_port_data.h"
#include "sh77xx_scif.h"

#include <string_view>

namespace cerfcore {

namespace {

constexpr uint32_t kPhysicalMask  = 0x1FFFFFFFu;
constexpr uint32_t kAreaShift     = 29u;
constexpr uint32_t kNoShadow      = 7u;
constexpr uint32_t kArea7         = 0x1C000000u;
constexpr uint32_t kDaModuleBytes = 6u;

struct PaRange {
    uint32_t first;
    uint32_t end;
};

constexpr PaRange kAdRange     = {kSh77xxAdcBase, kSh77xxAdcBase + kSh77xxAdcWindowBytes};
constexpr PaRange kAdDataRange = {kSh77xxAdcBase, kSh77xxAdcBase + kSh77xxAdcDataBytes};
constexpr PaRange kDaRange     = {kSh77xxDacBase, kSh77xxDacBase + kDaModuleBytes};
constexpr PaRange kPortRange   = {kSh77xxPfcBase, kSh77xxPortDataBase + kSh77xxPortCount * kSh77xxPortDataStride};
constexpr PaRange kIrdaRange   = {kSh77xxIrdaBase, kSh77xxIrdaBase + kSh77xxScifWindowBytes};
constexpr PaRange kScifRange   = {kSh77xxScif2Base, kSh77xxScif2Base + kSh77xxScifWindowBytes};
constexpr PaRange kDmaReachable[] = {kAdRange, kDaRange, kPortRange, kIrdaRange, kScifRange};

bool Inside(const PaRange& range, uint32_t pa, uint32_t bytes) { return pa >= range.first && pa + bytes <= range.end; }

bool DmaReachable(uint32_t pa, uint32_t bytes) {
    for (const PaRange& range : kDmaReachable) {
        if (Inside(range, pa, bytes)) return true;
    }
    return false;
}

}

uint32_t Sh77xxDmaAddressMap::BusAddress(uint32_t channel, uint32_t addr, uint32_t bytes, const char* side) const {
    if (addr % bytes != 0u) {
        emu_.Get<Fatal>().Die("Sh77xxDmac: channel %u %s address 0x%08X is not aligned to %u bytes", channel, side,
                              addr, bytes);
    }
    const uint32_t pa = addr & kPhysicalMask;
    if (pa >= kArea7) {
        emu_.Get<Fatal>().Die("Sh77xxDmac: channel %u %s address 0x%08X is in reserved area 7", channel, side, addr);
    }
    if ((addr >> kAreaShift) == kNoShadow) {
        emu_.Get<Fatal>().Die("Sh77xxDmac: channel %u %s address 0x%08X is in no area of the physical map", channel,
                              side, addr);
    }
    const bool in_area1 = pa >= kSh77xxArea1PaBase && pa - kSh77xxArea1PaBase < kSh77xxArea1Bytes;
    if (in_area1 && !DmaReachable(pa, bytes) && !ReachesChipModule(pa, bytes)) {
        emu_.Get<Fatal>().Die("Sh77xxDmac: channel %u %s address 0x%08X is an on-chip register the DMAC may not access",
                              channel, side, addr);
    }
    return pa;
}

bool Sh77xxDmaAddressMap::NamesDataRegister(Sh77xxDmaRequest source, uint32_t sar, uint32_t dar) const {
    const uint32_t sar_pa = sar & kPhysicalMask;
    const uint32_t dar_pa = dar & kPhysicalMask;
    switch (source) {
        case Sh77xxDmaRequest::kIrdaTx: return dar_pa == kSh77xxIrdaBase + kSh77xxScifTransmitOffset;
        case Sh77xxDmaRequest::kIrdaRx: return sar_pa == kSh77xxIrdaBase + kSh77xxScifReceiveOffset;
        case Sh77xxDmaRequest::kScifTx: return dar_pa == kSh77xxScif2Base + kSh77xxScifTransmitOffset;
        case Sh77xxDmaRequest::kScifRx: return sar_pa == kSh77xxScif2Base + kSh77xxScifReceiveOffset;
        case Sh77xxDmaRequest::kAd: return Inside(kAdDataRange, sar_pa, 1u);
        default: return true;
    }
}

}

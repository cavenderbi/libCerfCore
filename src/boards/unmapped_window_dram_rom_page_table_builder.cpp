#include "unmapped_window_dram_rom_page_table_builder.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../core/board_config.h"
#include "ram_param.h"

namespace cerfcore {

void UnmappedWindowDramRomPageTableBuilder::OnReady() {
    if (rom_from_entry_xip_) {
        rom_ = PlaceEntryXipRom(dram_.region.va_base, dram_.region.size);
    }
}

void UnmappedWindowDramRomPageTableBuilder::SetRamParamDram(uint32_t pa_base, uint32_t window_bytes,
                                                            DramRepeat repeat) {
    const uint32_t bytes   = emu_.Get<BoardConfig>().Enum(BoardConfigParamId::Ram, kRamBytes);
    const bool     repeats = repeat == DramRepeat::kPowerOfTwoSize && (bytes & (bytes - 1u)) == 0u;
    dram_ = CachedWindowDram(pa_base, bytes, repeats ? window_bytes : 0u, window_bytes);
}

void UnmappedWindowDramRomPageTableBuilder::SetDramCardBank(uint32_t pa_base, uint32_t window_bytes) {
    card_ = DramCardBank(pa_base, 0u, window_bytes);
}

std::vector<DramRegion> UnmappedWindowDramRomPageTableBuilder::CachedDramRegions() const {
    std::vector<DramRegion> r;
    dram_.AddCachedTo(r);
    card_.AddCachedTo(r);
    return r;
}

std::vector<BackedRegion> UnmappedWindowDramRomPageTableBuilder::BackedMemoryRegions() const {
    std::vector<BackedRegion> r;
    dram_.AddBackedTo(r);
    card_.AddBackedTo(r);
    r.push_back({ rom_.va_base, rom_.pa_base, rom_.size, MemoryKind::kRom });
    return r;
}

std::vector<DramRegion> UnmappedWindowDramRomPageTableBuilder::MappedVaSpans() const {
    std::vector<DramRegion> r;
    dram_.AddMappedTo(r);
    card_.AddMappedTo(r);
    r.push_back(rom_);
    return r;
}

}

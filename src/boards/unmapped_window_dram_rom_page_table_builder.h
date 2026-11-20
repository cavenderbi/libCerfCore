#pragma once

#include "unmapped_window_page_table_builder.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

enum class DramRepeat {
    kNever,
    kPowerOfTwoSize,
};

class UnmappedWindowDramRomPageTableBuilder : public UnmappedWindowPageTableBuilder {
public:
    UnmappedWindowDramRomPageTableBuilder(CerfEmulator& emu, const UnmappedWindowDram& dram,
                                          const DramRegion& rom)
        : UnmappedWindowPageTableBuilder(emu), dram_(dram), rom_(rom),
          rom_from_entry_xip_(false) {}

    UnmappedWindowDramRomPageTableBuilder(CerfEmulator& emu, const UnmappedWindowDram& dram)
        : UnmappedWindowPageTableBuilder(emu), dram_(dram), rom_{},
          rom_from_entry_xip_(true) {}

    explicit UnmappedWindowDramRomPageTableBuilder(CerfEmulator& emu)
        : UnmappedWindowPageTableBuilder(emu), dram_{}, rom_{}, rom_from_entry_xip_(true) {}

    void OnReady() override;

    std::vector<DramRegion>   CachedDramRegions()   const final;
    std::vector<BackedRegion> BackedMemoryRegions() const final;
    std::vector<DramRegion>   MappedVaSpans()       const final;

protected:
    void SetRamParamDram(uint32_t pa_base, uint32_t window_bytes, DramRepeat repeat);
    void SetDramCardBank(uint32_t pa_base, uint32_t window_bytes);

private:
    UnmappedWindowDram dram_;
    UnmappedWindowDram card_{};
    DramRegion         rom_;
    const bool         rom_from_entry_xip_;
};

}

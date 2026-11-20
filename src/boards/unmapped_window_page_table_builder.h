#pragma once

#include "page_table_builder.h"
#include "unmapped_window_dram.h"

#include <cerfengine/unmapped_windows.h>

#include <cstdint>

namespace cerfcore {

struct ParsedRom;
struct ParsedROMHDR;

class UnmappedWindowPageTableBuilder : public PageTableBuilder {
public:
    using PageTableBuilder::PageTableBuilder;

    uint32_t VaToPa(uint32_t va) const final;

    DramRegion EntryXipRomRegion() const final;

    uint32_t UnmappedWindowVa(uint32_t pa, uint32_t size) const final;

    static constexpr uint32_t CachedWindowVa(uint32_t pa) { return cerfengine::UnmappedWindows::kCachedBase | pa; }

    static constexpr UnmappedWindowDram CachedWindowDram(uint32_t pa_base, uint32_t bytes, uint32_t decode_span,
                                                         uint32_t mapped_span) {
        return { { CachedWindowVa(pa_base), pa_base, bytes }, decode_span, mapped_span };
    }

protected:
    const ParsedRom& PrimaryRom() const;

    UnmappedWindowDram DramCardBank(uint32_t pa_base, uint32_t decode_span, uint32_t mapped_span) const;

    DramRegion CachedWindowRomSpan(uint32_t va_first, uint32_t va_end) const;

    void RequireRamEndWithin(const ParsedROMHDR& hdr, uint32_t dram_va,
                             uint32_t dram_size) const;

    DramRegion PlaceEntryXipRom(uint32_t dram_va, uint32_t dram_size) const;

private:
    const ParsedROMHDR& EntryXipHeader() const;
};

}

#include "page_table_builder.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>

#include <typeinfo>

namespace cerfcore {

uint32_t PageTableBuilder::InitStackTopPa() const {
    emu_.Get<Fatal>().Die("PageTableBuilder::InitStackTopPa: this board declares no "
                          "bootloader-handoff SP");
}

DramRegion PageTableBuilder::EntryXipRomRegion() const {
    emu_.Get<Fatal>().Die("%s declares no entry-XIP ROM region", typeid(*this).name());
}

std::vector<DramRegion> PageTableBuilder::MappedVaSpans() const {
    emu_.Get<Fatal>().Die("%s declares no mapped VA spans", typeid(*this).name());
}

uint32_t PageTableBuilder::UnmappedWindowVa(uint32_t, uint32_t) const { return 0u; }

}

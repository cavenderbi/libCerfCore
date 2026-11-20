#include "unmapped_window_dram.h"

namespace cerfcore {

void UnmappedWindowDram::AddCachedTo(std::vector<DramRegion>& regions) const {
    if (region.size != 0u) regions.push_back(region);
}

void UnmappedWindowDram::AddBackedTo(std::vector<BackedRegion>& regions) const {
    if (region.size != 0u) {
        regions.push_back({ region.va_base, region.pa_base, region.size, MemoryKind::kRam, decode_span });
    }
}

void UnmappedWindowDram::AddMappedTo(std::vector<DramRegion>& spans) const {
    if (region.size != 0u) spans.push_back({ region.va_base, region.pa_base, mapped_span });
}

}

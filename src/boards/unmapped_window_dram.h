#pragma once

#include "page_table_builder.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

struct UnmappedWindowDram {
    DramRegion region{};
    uint32_t   decode_span = 0;
    uint32_t   mapped_span = 0;

    void AddCachedTo(std::vector<DramRegion>& regions) const;
    void AddBackedTo(std::vector<BackedRegion>& regions) const;
    void AddMappedTo(std::vector<DramRegion>& spans) const;
};

}

#include "rom_toc_lookup.h"

#include "rom_record_layout.h"
#include "../core/ascii_case.h"
#include <cerfcore/core/byte_order.h>

namespace cerfcore {

namespace cerf::rom_toc {

const ParsedTOCentry* FindModule(const ParsedTOC& toc, std::string_view name) {
    for (const auto& m : toc.modules)
        if (cerf::AsciiEqualIgnoreCase(m.lpszFileName, name)) return &m;
    return nullptr;
}

bool ReadSubsystemVersion(std::span<const uint8_t> flat, const ParsedXipRegion& xip,
                          const ParsedTOCentry& module, uint16_t& major, uint16_t& minor) {
    if (module.ulE32Offset < xip.load_offset) return false;
    const size_t e32_off = size_t(module.ulE32Offset - xip.load_offset);
    if (e32_off + kE32OffSubsysMinor + 2u > flat.size()) return false;
    major = cerf::le::U16(flat.data(), e32_off + kE32OffSubsysMajor);
    minor = cerf::le::U16(flat.data(), e32_off + kE32OffSubsysMinor);
    return true;
}

}

}

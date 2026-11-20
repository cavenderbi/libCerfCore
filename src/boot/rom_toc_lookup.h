#pragma once

#include <cerfcore/boot/rom_parser_service.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace cerfcore {

namespace cerf::rom_toc {

const ParsedTOCentry* FindModule(const ParsedTOC& toc, std::string_view name);

bool ReadSubsystemVersion(std::span<const uint8_t> flat, const ParsedXipRegion& xip,
                          const ParsedTOCentry& module, uint16_t& major, uint16_t& minor);

}

}

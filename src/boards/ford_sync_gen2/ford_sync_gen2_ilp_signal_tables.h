#pragma once

#include "ford_sync_gen2_ilp_descriptors.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace cerfcore::cerf_ford_sync_gen2_ilp_detail {

struct GroundedSignal {
    uint32_t sigid;
    uint8_t bits;
};

// SyncGen2.bcf defines filter metadata independently of hardware status values.
// Accepting a known filter must not imply that its value is modeled or reported.
inline constexpr std::size_t kSignalMetadataCount =
    std::size(kAssociation0Bits) + std::size(kAssociation1Bits) + std::size(kAssociation2Bits);
inline constexpr auto kGroundedSignals = [] {
    std::array<GroundedSignal, kSignalMetadataCount> result{};
    std::size_t next = 0;
    auto append = [&](uint32_t base, const auto& bits) {
        for (std::size_t i = 0; i < std::size(bits); ++i)
            result[next++] = {base + static_cast<uint32_t>(i), bits[i]};
    };
    append(0x00000000u, kAssociation0Bits);
    append(0x01000000u, kAssociation1Bits);
    append(0x02000000u, kAssociation2Bits);
    return result;
}();

}

#include "casio_cassiopeia_a10_register_00e.h"

#include <algorithm>
#include <cstdint>

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10Register00E);

namespace {

constexpr uint16_t kStoredValues[] = {0x0000u, 0x0002u, 0x0003u, 0x0006u, 0x0007u, 0x0017u, 0x001Bu, 0xFFFFu};
constexpr uint16_t kSetMasks[]     = {0x0006u, 0x0008u, 0x0010u};
constexpr uint16_t kClearMask      = 0x0008u;

}

bool CasioCassiopeiaA10Register00E::Accepts(uint16_t value) const {
    if (std::ranges::find(kStoredValues, value) != std::ranges::end(kStoredValues)) return true;
    for (const uint16_t mask : kSetMasks) {
        if (value == static_cast<uint16_t>(Value() | mask)) return true;
    }
    return value == static_cast<uint16_t>(Value() & ~kClearMask);
}

}

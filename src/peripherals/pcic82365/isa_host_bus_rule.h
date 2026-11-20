#pragma once

#include <cstdint>

namespace cerfcore {

class IsaHostBusRule {
public:
    virtual bool AllowsCycle(bool sixteen_bit_device, unsigned bytes, uint32_t off) const = 0;

protected:
    ~IsaHostBusRule() = default;
};

struct UnrestrictedIsaHost {
    static constexpr bool kRestricted = false;
};

}

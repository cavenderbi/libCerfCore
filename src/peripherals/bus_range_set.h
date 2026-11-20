#pragma once

#include <cstdint>
#include <vector>

namespace cerfcore {

class BusRangeSet {
public:
    struct Range {
        uint32_t base;
        uint32_t end;
        bool     on;
    };

    uint32_t     Add(uint32_t base, uint32_t end, bool on);
    bool         Set(uint32_t id, bool on);
    const Range* Overlapping(uint32_t base, uint64_t end) const;
    void         AppendStates(std::vector<bool>& key) const;

private:
    std::vector<Range> ranges_;
};

}

#include "bus_range_set.h"

namespace cerfcore {

uint32_t BusRangeSet::Add(uint32_t base, uint32_t end, bool on) {
    ranges_.push_back({base, end, on});
    return static_cast<uint32_t>(ranges_.size() - 1u);
}

bool BusRangeSet::Set(uint32_t id, bool on) {
    Range& r = ranges_[id];
    if (r.on == on) return false;
    r.on = on;
    return true;
}

const BusRangeSet::Range* BusRangeSet::Overlapping(uint32_t base, uint64_t end) const {
    for (const Range& r : ranges_) {
        if (base < r.end && r.base < end) return &r;
    }
    return nullptr;
}

void BusRangeSet::AppendStates(std::vector<bool>& key) const {
    for (const Range& r : ranges_) key.push_back(r.on);
}

}

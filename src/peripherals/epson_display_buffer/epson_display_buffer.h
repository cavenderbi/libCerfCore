#pragma once

#include <cerfcore/core/byte_order.h>

#include <cstdint>
#include <vector>

namespace cerfcore {

class StateReader;
class StateWriter;

class EpsonDisplayBuffer {
public:
    void Allocate(uint32_t bytes);

    uint32_t       Size() const { return static_cast<uint32_t>(bytes_.size()); }
    const uint8_t* Data() const { return bytes_.data(); }
    uint32_t       Wrap(uint32_t off) const { return mask_ != 0u ? (off & mask_) : (off % Size()); }

    uint32_t Load(uint32_t off, uint32_t width) const {
        if (InRange(off, width)) return static_cast<uint32_t>(cerf::le::UN(&bytes_[off], width));
        return LoadWrapped(off, width);
    }

    void Store(uint32_t off, uint32_t value, uint32_t width) {
        if (InRange(off, width)) {
            cerf::le::PutN(&bytes_[off], value, width);
            return;
        }
        StoreWrapped(off, value, width);
    }

    void SaveState(StateWriter& w, const char* name) const;
    void RestoreState(StateReader& r, const char* name);

private:
    bool     InRange(uint32_t off, uint32_t width) const { return off < Size() && width <= Size() - off; }
    uint32_t LoadWrapped(uint32_t off, uint32_t width) const;
    void     StoreWrapped(uint32_t off, uint32_t value, uint32_t width);

    std::vector<uint8_t> bytes_;
    uint32_t             mask_ = 0;
};

}

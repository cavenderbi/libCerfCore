#include "epson_display_buffer.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

void EpsonDisplayBuffer::Allocate(uint32_t bytes) {
    bytes_.assign(bytes, 0u);
    mask_ = (bytes & (bytes - 1u)) == 0u ? bytes - 1u : 0u;
}

uint32_t EpsonDisplayBuffer::LoadWrapped(uint32_t off, uint32_t width) const {
    const uint32_t r = Wrap(off);
    if (InRange(r, width)) return static_cast<uint32_t>(cerf::le::UN(&bytes_[r], width));
    uint32_t v = 0;
    for (uint32_t i = 0; i < width; ++i) v |= uint32_t(bytes_[Wrap(r + i)]) << (8u * i);
    return v;
}

void EpsonDisplayBuffer::StoreWrapped(uint32_t off, uint32_t value, uint32_t width) {
    const uint32_t r = Wrap(off);
    if (InRange(r, width)) {
        cerf::le::PutN(&bytes_[r], value, width);
        return;
    }
    for (uint32_t i = 0; i < width; ++i) bytes_[Wrap(r + i)] = static_cast<uint8_t>(value >> (8u * i));
}

void EpsonDisplayBuffer::SaveState(StateWriter& w, const char* name) const {
    w.WriteBytes(name, bytes_.data(), bytes_.size());
}

void EpsonDisplayBuffer::RestoreState(StateReader& r, const char* name) {
    r.ReadBytes(name, bytes_.data(), bytes_.size());
}

}

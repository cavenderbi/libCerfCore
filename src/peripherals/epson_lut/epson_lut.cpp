#include "epson_lut.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

void EpsonLutTable::Rgb(uint32_t index, uint8_t& r4, uint8_t& g4, uint8_t& b4) const {
    r4 = entries_[index & 0xFFu][0];
    g4 = entries_[index & 0xFFu][1];
    b4 = entries_[index & 0xFFu][2];
}

void EpsonLutTable::Store(uint8_t index, const uint8_t (&rgb)[3]) {
    entries_[index][0] = rgb[0];
    entries_[index][1] = rgb[1];
    entries_[index][2] = rgb[2];
}

void EpsonLutTable::SaveState(StateWriter& w, const char* name) const {
    w.WriteBytes(name, entries_, sizeof(entries_));
}

void EpsonLutTable::RestoreState(StateReader& r, const char* name) {
    r.ReadBytes(name, entries_, sizeof(entries_));
}

uint8_t EpsonLutPort::Read(const EpsonLutTable& table) {
    const uint8_t v = static_cast<uint8_t>(table.Component(index_, component_) << 4);
    Step();
    return v;
}

void EpsonLutPort::Write(uint8_t value, std::span<EpsonLutTable* const> targets) {
    const uint8_t comp = static_cast<uint8_t>(value >> 4);
    if (component_ < 2) {
        rgb_latch_[component_] = comp;
    } else {
        const uint8_t rgb[3] = {rgb_latch_[0], rgb_latch_[1], comp};
        for (EpsonLutTable* table : targets) table->Store(index_, rgb);
    }
    Step();
}

void EpsonLutPort::PointAt(uint8_t index) {
    index_     = index;
    component_ = 0;
}

void EpsonLutPort::ResetPointer() {
    index_        = 0;
    component_    = 0;
    rgb_latch_[0] = rgb_latch_[1] = 0;
}

void EpsonLutPort::Step() {
    if (++component_ == 3) {
        component_ = 0;
        index_++;
    }
}

void EpsonLutPort::SaveState(StateWriter& w) const {
    w.Write("lut_index", index_);
    w.Write("lut_component", component_);
    w.WriteBytes("lut_rgb_latch", rgb_latch_, sizeof(rgb_latch_));
}

void EpsonLutPort::RestoreState(StateReader& r) {
    r.Read("lut_index", index_);
    r.Read("lut_component", component_);
    r.ReadBytes("lut_rgb_latch", rgb_latch_, sizeof(rgb_latch_));
}

}

#include "pcic82365_window_registers.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

enum : uint8_t {
    kRegWindowEnable    = 0x06,
    kRegIoWindowControl = 0x07,
    kRegIoMapFirst      = 0x08,
    kRegIoMapLast       = 0x0F,
    kRegMemMapFirst     = 0x10,
    kRegMemMapLast      = 0x35,
    kRegIoOffsetFirst   = 0x36,
    kRegIoOffsetLast    = 0x39,
    kRegMemPageFirst    = 0x40,
    kRegMemPageLast     = 0x44,
};

constexpr uint8_t kWinMemcs16Decode = 0x20u;
constexpr uint8_t kWinIoMap0Enable  = 0x40u;
constexpr uint8_t kWinIoMap1Enable  = 0x80u;

constexpr uint8_t kMohRegActive    = 0x40u;
constexpr uint8_t kMohWriteProtect = 0x80u;
constexpr uint8_t kMshDataSize16   = 0x80u;
constexpr uint8_t kMemEndHighSlot  = 3u;

constexpr uint8_t kIoCtlDataSize     = 0x01u;
constexpr uint8_t kIoCtlIois16Source = 0x02u;

bool IsMemMapReg(uint8_t index) {
    return index >= kRegMemMapFirst && index <= kRegMemMapLast && (index & 7u) <= 5u;
}

bool IsIoMapReg(uint8_t index) {
    return index >= kRegIoMapFirst && index <= kRegIoMapLast;
}

bool IsIoOffsetReg(uint8_t index) {
    return index >= kRegIoOffsetFirst && index <= kRegIoOffsetLast;
}

}

bool Pcic82365WindowRegisters::Owns(uint8_t index) const {
    if (index == kRegWindowEnable || index == kRegIoWindowControl) return true;
    if (IsIoMapReg(index) || IsMemMapReg(index) || IsIoOffsetReg(index)) return true;
    return rules_.window_page_registers && index >= kRegMemPageFirst && index <= kRegMemPageLast;
}

bool Pcic82365WindowRegisters::Read(uint8_t index, uint8_t* value) const {
    if (!Owns(index)) return false;
    if (index == kRegWindowEnable) {
        *value = reg_window_enable_;
    } else if (index == kRegIoWindowControl) {
        *value = reg_io_window_control_;
    } else if (IsMemMapReg(index)) {
        *value = mem_reg_[(index - kRegMemMapFirst) >> 3][index & 7u];
    } else if (IsIoMapReg(index)) {
        const int win = (index - kRegIoMapFirst) >= 4 ? 1 : 0;
        const uint8_t slot = (index - kRegIoMapFirst) & 3u;
        if (slot == 0u)
            *value = io_start_lo_[win];
        else if (slot == 1u)
            *value = io_start_hi_[win];
        else if (slot == 2u)
            *value = io_end_lo_[win];
        else
            *value = io_end_hi_[win];
    } else if (IsIoOffsetReg(index)) {
        const int win = (index - kRegIoOffsetFirst) >= 2 ? 1 : 0;
        *value = ((index - kRegIoOffsetFirst) & 1u) ? io_off_hi_[win] : io_off_lo_[win];
    } else {
        *value = mem_page_[index - kRegMemPageFirst];
    }
    return true;
}

bool Pcic82365WindowRegisters::Store(uint8_t index, uint8_t value) {
    if (!Owns(index)) return false;
    if (index == kRegWindowEnable) {
        reg_window_enable_ = static_cast<uint8_t>(value & ~rules_.window_enable_reserved);
    } else if (index == kRegIoWindowControl) {
        reg_io_window_control_ = value;
    } else if (IsMemMapReg(index)) {
        if ((index & 7u) == kMemEndHighSlot)
            value = static_cast<uint8_t>(value & ~rules_.memory_end_high_reserved);
        mem_reg_[(index - kRegMemMapFirst) >> 3][index & 7u] = value;
    } else if (IsIoMapReg(index)) {
        const int win = (index - kRegIoMapFirst) >= 4 ? 1 : 0;
        switch ((index - kRegIoMapFirst) & 3u) {
            case 0:
                io_start_lo_[win] = value;
                break;
            case 1:
                io_start_hi_[win] = value;
                break;
            case 2:
                io_end_lo_[win] = value;
                break;
            case 3:
                io_end_hi_[win] = value;
                break;
        }
    } else if (IsIoOffsetReg(index)) {
        const int win = (index - kRegIoOffsetFirst) >= 2 ? 1 : 0;
        if ((index - kRegIoOffsetFirst) & 1u) io_off_hi_[win] = value;
        else io_off_lo_[win] = static_cast<uint8_t>(value & ~rules_.io_offset_low_reserved);
    } else {
        mem_page_[index - kRegMemPageFirst] = value;
    }
    return true;
}

void Pcic82365WindowRegisters::Reset() {
    reg_window_enable_     = 0u;
    reg_io_window_control_ = 0u;
    for (int win = 0; win < 2; ++win) {
        io_start_lo_[win] = io_start_hi_[win] = 0u;
        io_end_lo_[win]   = io_end_hi_[win]   = 0u;
        io_off_lo_[win]   = io_off_hi_[win]   = 0u;
    }
    for (auto& win : mem_reg_)
        for (auto& reg : win) reg = 0u;
    for (auto& page : mem_page_) page = 0u;
}

void Pcic82365WindowRegisters::ResetConfiguration() {
    reg_window_enable_ = static_cast<uint8_t>(reg_window_enable_ & kWinMemcs16Decode);
    reg_io_window_control_ = 0u;
    for (int win = 0; win < 2; ++win) {
        io_start_lo_[win] = io_start_hi_[win] = 0u;
        io_end_lo_[win]   = io_end_hi_[win]   = 0u;
    }
    for (auto& win : mem_reg_)
        for (auto& reg : win) reg = 0u;
}

bool Pcic82365WindowRegisters::MapIo(uint32_t bus_io, uint32_t* card_io, PcmciaIoWidth* width) const {
    auto u16 = [](uint8_t lo, uint8_t hi) -> uint32_t {
        return static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 8);
    };
    for (int win : {1, 0}) {
        const uint8_t enable_bit = (win == 1) ? kWinIoMap1Enable : kWinIoMap0Enable;
        if (!(reg_window_enable_ & enable_bit)) continue;
        const uint32_t start = u16(io_start_lo_[win], io_start_hi_[win]);
        const uint32_t end   = u16(io_end_lo_[win],   io_end_hi_[win]);
        if (bus_io < start || bus_io > end) continue;
        const uint32_t offset = u16(io_off_lo_[win], io_off_hi_[win]) & 0xFFFEu;
        *card_io = (bus_io + offset) & 0xFFFFu;
        const uint8_t control = static_cast<uint8_t>(reg_io_window_control_ >> (4 * win));
        if ((control & kIoCtlIois16Source) != 0u)
            *width = PcmciaIoWidth::kCardIois16;
        else if ((control & kIoCtlDataSize) != 0u)
            *width = PcmciaIoWidth::kSixteenBit;
        else
            *width = PcmciaIoWidth::kEightBit;
        return true;
    }
    return false;
}

bool Pcic82365WindowRegisters::MapMem(uint32_t bus_off, uint32_t* card_addr,
                              bool* attribute, bool* writable, bool* sixteen_bit) const {
    const uint32_t host_page = bus_off >> 12;
    for (int win = 0; win < 5; ++win) {
        if (!(reg_window_enable_ & (1u << win))) continue;
        const uint8_t* r = mem_reg_[win];
        uint32_t start, end;
        WindowPages(win, &start, &end);
        if (host_page < start || host_page > end) continue;
        const uint32_t offset = static_cast<uint32_t>(r[4]) | (static_cast<uint32_t>(r[5] & 0x3Fu) << 8);
        *card_addr = (bus_off + (offset << 12)) & 0x3FFFFFFu;
        *attribute   = (r[5] & kMohRegActive)    != 0u;
        *writable    = (r[5] & kMohWriteProtect) == 0u;
        *sixteen_bit = (r[1] & kMshDataSize16)   != 0u;
        return true;
    }
    return false;
}

bool Pcic82365WindowRegisters::SixteenBitWindowDecodes(uint32_t bus_off, unsigned decode_shift) const {
    const uint32_t unit = bus_off >> decode_shift;
    for (int win = 0; win < 5; ++win) {
        if (!(reg_window_enable_ & (1u << win))) continue;
        if ((mem_reg_[win][1] & kMshDataSize16) == 0u) continue;
        uint32_t start, end;
        WindowPages(win, &start, &end);
        if (unit >= (start << 12) >> decode_shift && unit <= (end << 12) >> decode_shift) return true;
    }
    return false;
}

void Pcic82365WindowRegisters::WindowPages(int win, uint32_t* start, uint32_t* end) const {
    const uint8_t* r = mem_reg_[win];
    const uint32_t pg = static_cast<uint32_t>(mem_page_[win]) << 12;
    *start = pg | static_cast<uint32_t>(r[0]) | (static_cast<uint32_t>(r[1] & 0x0Fu) << 8);
    *end   = pg | static_cast<uint32_t>(r[2]) | (static_cast<uint32_t>(r[3] & 0x0Fu) << 8);
}

void Pcic82365WindowRegisters::SaveState(StateWriter& w) const {
    w.Write("reg_window_enable", reg_window_enable_);
    w.Write("reg_io_window_control", reg_io_window_control_);
    w.WriteBytes("io_start_lo", io_start_lo_, sizeof(io_start_lo_));
    w.WriteBytes("io_start_hi", io_start_hi_, sizeof(io_start_hi_));
    w.WriteBytes("io_end_lo", io_end_lo_, sizeof(io_end_lo_));
    w.WriteBytes("io_end_hi", io_end_hi_, sizeof(io_end_hi_));
    w.WriteBytes("io_off_lo", io_off_lo_, sizeof(io_off_lo_));
    w.WriteBytes("io_off_hi", io_off_hi_, sizeof(io_off_hi_));
    w.WriteBytes("mem_reg", mem_reg_, sizeof(mem_reg_));
    w.WriteBytes("mem_page", mem_page_, sizeof(mem_page_));
}

void Pcic82365WindowRegisters::RestoreState(StateReader& r) {
    r.Read("reg_window_enable", reg_window_enable_);
    r.Read("reg_io_window_control", reg_io_window_control_);
    r.ReadBytes("io_start_lo", io_start_lo_, sizeof(io_start_lo_));
    r.ReadBytes("io_start_hi", io_start_hi_, sizeof(io_start_hi_));
    r.ReadBytes("io_end_lo", io_end_lo_, sizeof(io_end_lo_));
    r.ReadBytes("io_end_hi", io_end_hi_, sizeof(io_end_hi_));
    r.ReadBytes("io_off_lo", io_off_lo_, sizeof(io_off_lo_));
    r.ReadBytes("io_off_hi", io_off_hi_, sizeof(io_off_hi_));
    r.ReadBytes("mem_reg", mem_reg_, sizeof(mem_reg_));
    r.ReadBytes("mem_page", mem_page_, sizeof(mem_page_));
}

}

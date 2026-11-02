#pragma once

#include "../pcmcia/pcmcia_io_width.h"

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

struct ExcaWindowRules {
    uint8_t window_enable_reserved;
    uint8_t memory_end_high_reserved;
    uint8_t io_offset_low_reserved;
    bool    window_page_registers;
};

class Pcic82365WindowRegisters {
public:
    explicit Pcic82365WindowRegisters(ExcaWindowRules rules) : rules_(rules) {}

    bool Owns(uint8_t index) const;
    bool Read(uint8_t index, uint8_t* value) const;
    bool Store(uint8_t index, uint8_t value);

    void Reset();
    void ResetConfiguration();

    uint8_t WindowEnable() const { return reg_window_enable_; }

    bool MapIo(uint32_t bus_io, uint32_t* card_io, PcmciaIoWidth* width) const;
    bool MapMem(uint32_t bus_off, uint32_t* card_addr,
                bool* attribute, bool* writable, bool* sixteen_bit) const;
    bool SixteenBitWindowDecodes(uint32_t bus_off, unsigned decode_shift) const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    void WindowPages(int win, uint32_t* start, uint32_t* end) const;

    ExcaWindowRules rules_;

    uint8_t reg_window_enable_     = 0u;
    uint8_t reg_io_window_control_ = 0u;
    uint8_t io_start_lo_[2] = {};
    uint8_t io_start_hi_[2] = {};
    uint8_t io_end_lo_  [2] = {};
    uint8_t io_end_hi_  [2] = {};
    uint8_t io_off_lo_  [2] = {};
    uint8_t io_off_hi_  [2] = {};
    uint8_t mem_reg_[5][6]  = {};
    uint8_t mem_page_[5]    = {};
};

}

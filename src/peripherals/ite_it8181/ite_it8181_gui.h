#pragma once

#include "ite_it8181_blitter.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

class CerfEmulator;
class StateWriter;
class StateReader;

class IteIt8181Gui {
public:
    static constexpr uint32_t kWindowBytes = 0x8000u;

    IteIt8181Gui(CerfEmulator& emu, std::vector<uint8_t>& vram, const IteIt8181Vga& vga)
        : emu_(emu), blitter_(emu, vram, vga) {}

    void PowerOnReset();

    uint32_t Read(uint32_t off);
    void     Write(uint32_t off, uint32_t value);

    bool     CursorShown() const;
    uint32_t CursorX() const { return cursor_pos_ & 0xFFFu; }
    uint32_t CursorY() const { return (cursor_pos_ >> 16) & 0xFFFu; }
    bool     CursorColor0White() const { return cursor_color0_ != 0u; }
    bool     CursorColor1White() const { return cursor_color1_ != 0u; }

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    static constexpr uint32_t kSuspendRegs = 12u;

    uint32_t* BlitRegister(uint32_t off);
    void      StartCommand(uint32_t bc);
    void      PushHostData(uint32_t value);
    void      WriteCursorColor(uint32_t& color, uint32_t off, uint32_t value);

    CerfEmulator&                emu_;
    IteIt8181Blitter             blitter_;
    IteIt8181Blitter::Registers  regs_;
    uint32_t                     suspend_regs_[kSuspendRegs] = {};
    uint32_t                     cursor_ctrl_   = 0;
    uint32_t                     cursor_color0_ = 0;
    uint32_t                     cursor_color1_ = 0;
    uint32_t                     cursor_pos_    = 0;
    std::vector<uint8_t>         host_;
    uint32_t                     host_needed_ = 0;
};

}

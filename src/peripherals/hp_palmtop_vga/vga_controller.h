#pragma once

#include <cerfcore/host/frame_source.h>
#include "../vga/vga_register_file.h"
#include "../vga/vga_retrace.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

class CerfEmulator;
class StateWriter;
class StateReader;

class VgaController : public FrameSource {
public:
    explicit VgaController(CerfEmulator& emu);

    void Attach();
    void PowerOnReset();

    uint8_t ReadReg8(uint32_t port);
    void    WriteReg8(uint32_t port, uint8_t value);

    bool     LinearOffset(uint32_t card_offset, uint32_t& vram_offset) const;
    uint8_t* Vram() { return fb_.data(); }
    static constexpr uint32_t kVramBytes = 0x000C0000u;

    bool CurrentMode(uint32_t& w, uint32_t& h, uint32_t& bpp) const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

    bool HasFrame() override;
    void RenderInto(uint32_t* dib_bgra32, uint32_t width, uint32_t height) override;

private:
    uint8_t ReadSequencerData();
    void    WriteSequencerData(uint8_t value);
    void    WriteCrtcData(uint8_t value);
    uint8_t ReadPelMask();
    void    WritePelMask(uint8_t value);
    void    WriteBank(uint32_t port, uint8_t value);
    uint8_t ReadBank(uint32_t port) const;
    bool    ClockWritesUnlocked() const;
    GuestCycleClock::Rate DotClock() const;

    bool     PackedPixels() const;
    uint32_t Width() const;
    uint32_t Height() const;
    uint32_t StrideBytes() const;
    uint32_t StartAddress() const;
    uint32_t Bpp() const;

    CerfEmulator&   emu_;
    VgaRegisterFile regs_;
    VgaRetrace      retrace_;

    bool     new_mode_    = false;
    uint8_t  sr0d_old_    = 0;
    uint8_t  sr0d_new_    = 0;
    uint8_t  sr0e_old_    = 0;
    uint8_t  sr0e_new_    = 0;
    uint8_t  bank_write_  = 0;
    uint8_t  bank_read_   = 0;
    uint8_t  port_3db_    = 0;
    uint8_t  mem_clock_[2] = {};
    uint8_t  vid_clock_[2] = {};
    uint8_t  dac_command_ = 0;
    uint8_t  dac_reads_   = 0;

    std::vector<uint8_t> fb_;
};

}

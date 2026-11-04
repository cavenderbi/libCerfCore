#pragma once

#include "../vga/vga_register_file.h"
#include "../vga/vga_retrace.h"

#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class StateWriter;
class StateReader;

class IteIt8181Vga {
public:
    explicit IteIt8181Vga(CerfEmulator& emu) : emu_(emu), retrace_(emu) {}

    void Attach();
    void PowerOnReset();

    uint8_t Read(uint32_t port);
    void    Write(uint32_t port, uint8_t value);

    bool     PanelPowered() const;
    uint32_t Width() const;
    uint32_t Height() const;
    uint32_t StrideBytes() const;
    uint32_t StartByte() const;
    uint32_t BytesPerPixel() const;
    uint32_t PaletteXrgb(uint8_t index) const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    uint8_t ReadGraphicsData();
    void    WriteGraphicsData(uint8_t value);
    uint8_t ReadExtensionData();
    void    WriteExtensionData(uint8_t value);
    GuestCycleClock::Rate DotClock() const;

    CerfEmulator& emu_;

    VgaRegisterFile regs_;
    VgaRetrace      retrace_;
    uint8_t ema_[256]      = {};
    uint8_t ema_index_     = 0;
    bool    ema_enabled_   = false;
    bool    emb_enabled_   = false;
};

}

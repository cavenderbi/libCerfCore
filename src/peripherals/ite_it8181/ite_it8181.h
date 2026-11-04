#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include "../../lcd/display_mode_latch.h"
#include "ite_it8181_gui.h"
#include "ite_it8181_vga.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

class IteIt8181 : public Peripheral {
public:
    using Peripheral::Peripheral;

    struct Scanout {
        bool     shown  = false;
        uint32_t width  = 0;
        uint32_t height = 0;
        uint32_t stride = 0;
        uint32_t start  = 0;
        uint32_t bytes  = 0;
    };

    static constexpr uint32_t kVramBytes   = 0x00400000u;
    static constexpr uint32_t kCursorImage = 0x000FFF00u;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override { return base_; }
    uint32_t MmioSize() const override { return kWindowBytes; }

    uint8_t  ReadByte (uint32_t addr) override;
    uint16_t ReadHalf (uint32_t addr) override;
    uint32_t ReadWord (uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t  value) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;
    void     WriteWord(uint32_t addr, uint32_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

    Scanout        Display() const;
    const uint8_t* Vram() const { return vram_.data(); }
    uint32_t       PaletteXrgb(uint8_t index) const { return vga_.PaletteXrgb(index); }
    const IteIt8181Gui& Gui() const { return gui_; }

private:
    static constexpr uint32_t kWindowBytes = 0x00800000u;

    enum class Region : uint8_t { Config, Vram, Gui, Vga };
    struct Target {
        Region   region;
        uint32_t off;
    };

    Target   Decode(uint32_t addr, const char* op, uint64_t value) const;
    uint32_t ConfigRead(uint32_t off);
    void     ConfigWrite(uint32_t off, uint32_t value);
    void     PowerOnReset();
    void     PublishDisplay();

    uint32_t base_     = 0;
    uint8_t  revision_ = 0;
    uint32_t mba_      = 0;
    uint32_t gba_      = 0;
    uint32_t sba_      = 0;
    uint32_t test_     = 0;
    uint32_t standby_  = 0;
    uint32_t pll1_     = 0;
    uint32_t pll2_     = 0;

    std::vector<uint8_t> vram_;
    IteIt8181Vga         vga_{emu_};
    IteIt8181Gui         gui_{emu_, vram_, vga_};
    DisplayModeLatch     mode_latch_;
};

}

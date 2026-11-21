#pragma once

#include "../../lcd/display_mode_latch.h"
#include "../../socs/raster_scan_peripheral.h"
#include "../epson_lut/epson_lut.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

class S1d13705Config;

class S1d13705 : public RasterScanPeripheral {
public:
    using RasterScanPeripheral::RasterScanPeripheral;

    static constexpr uint32_t kDisplayBufferBytes = 0x14000u;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override { return mmio_base_; }
    uint32_t MmioSize() const override { return kWindowBytes; }

    uint8_t  ReadByte (uint32_t addr) override;
    uint16_t ReadHalf (uint32_t addr) override;
    uint32_t ReadWord (uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t  value) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;
    void     WriteWord(uint32_t addr, uint32_t value) override;

    bool     DisplayActive() const { return (Reg(0x03) & 0x3u) == 0x3u; }
    bool     PanelPowered() const  { return DisplayActive() && (Reg(0x03) & 0x08u) == 0u; }
    bool     Blanked() const       { return (Reg(0x02) & 0x08u) != 0u; }
    bool     Inverted() const      { return (Reg(0x02) & 0x01u) != 0u; }
    bool     Tft() const           { return (Reg(0x01) & 0x80u) != 0u; }
    bool     Tft12Bit() const      { return (Reg(0x01) & 0x01u) != 0u; }
    bool     Color() const         { return Tft() || (Reg(0x01) & 0x20u) != 0u; }
    uint32_t Bpp() const;
    uint32_t PanelWidth() const    { return ((Reg(0x04) & 0x7Fu) + 1u) * 8u; }
    uint32_t PanelHeight() const   { return PanelHeightField() + 1u; }
    uint32_t PanelHeightField() const { return ((Reg(0x06) & 0x3u) << 8) | Reg(0x05); }
    bool     Swivel() const        { return (Reg(0x1B) & 0x80u) != 0u; }
    uint32_t Screen1StartWord() const  { return (uint32_t(Reg(0x0D)) << 8) | Reg(0x0C); }
    uint32_t Screen2StartWord() const  { return (uint32_t(Reg(0x0F)) << 8) | Reg(0x0E); }
    uint32_t SwivelStartByte() const   { return ((Reg(0x10) & 0x1u) << 16) | Screen1StartWord(); }
    uint32_t AddressOffsetWords() const { return Reg(0x11); }
    uint32_t Screen1VerticalSize() const { return ((Reg(0x13) & 0x3u) << 8) | Reg(0x12); }
    uint32_t SwivelStrideBytes() const { return Reg(0x1C) != 0u ? Reg(0x1C) : 256u; }
    void     LutRgb(uint32_t index, uint8_t& r4, uint8_t& g4, uint8_t& b4) const { lut_table_.Rgb(index, r4, g4, b4); }
    const uint8_t* Vram() const { return vram_.data(); }

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

private:
    static constexpr uint32_t kWindowBytes = 0x20000u;
    static constexpr uint32_t kRegBase     = 0x1FFE0u;
    static constexpr uint32_t kRegCount    = 0x20u;

    uint8_t Reg(uint32_t off) const { return reg_[off]; }
    uint8_t RegRead(uint32_t off);
    void    RegWrite(uint32_t off, uint8_t value);
    void    WriteTiming(uint32_t off, uint8_t value);
    void    CheckOutputFormat() const;
    bool    ReservedFormat() const;
    bool    IsRegister(uint32_t off, uint32_t width) const;
    bool    IsBuffer(uint32_t off, uint32_t width) const;
    void    HardwareReset();
    void    PublishOnPanelEdge();
    void    RetimeScanLocked();
    void    OnClkiChange();

    bool                  DualPanel() const;
    uint64_t              LineTicks() const;
    uint32_t              DisplayLines() const;
    uint64_t              FrameTicks() const;
    GuestCycleClock::Rate PixelRate() const;
    uint8_t               VndStatusBit();

    ScanShape ScanShapeLocked() const override;
    bool      EdgeRaisesInterruptLocked(uint32_t, bool) const override { return false; }
    bool      FrameEdgesInertLocked() const override { return true; }
    void      FrameEdgeLocked(uint32_t) override {}
    void      ScanEdgesRan() override {}

    S1d13705Config*      config_    = nullptr;
    uint32_t             mmio_base_ = 0;
    uint8_t              reg_[kRegCount] = {};
    std::vector<uint8_t> vram_;
    EpsonLutTable        lut_table_;
    EpsonLutPort         lut_port_;
    DisplayModeLatch     mode_latch_;
};

}

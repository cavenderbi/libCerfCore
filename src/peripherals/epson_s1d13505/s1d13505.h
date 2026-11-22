#pragma once

#include "../../lcd/display_mode_latch.h"
#include "../../socs/raster_scan_peripheral.h"
#include "../epson_display_buffer/epson_display_buffer.h"
#include "../epson_ink_cursor/epson_ink_cursor_layer.h"
#include "../epson_lut/epson_lut.h"

#include <cstdint>

namespace cerfcore {

class S1d13505Config;

class S1d13505 : public RasterScanPeripheral {
public:
    using RasterScanPeripheral::RasterScanPeripheral;

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

    bool     PanelPowered() const;
    bool     FifoDisabled() const { return (Reg(0x23) & 0x80u) != 0u; }
    bool     Tft() const { return (Reg(0x02) & 0x01u) != 0u; }
    bool     Color() const { return Tft() || (Reg(0x02) & 0x04u) != 0u; }
    uint32_t Bpp() const;
    uint32_t PanelWidth() const { return ((Reg(0x04) & 0x7Fu) + 1u) * 8u; }
    uint32_t PanelHeight() const { return DualPanel() ? DisplayLines() * 2u : DisplayLines(); }
    uint32_t Screen1Lines() const { return TenBits(0x0E) + 1u; }
    uint32_t Screen1StartByte() const { return StartWord(0x10) * 2u; }
    uint32_t Screen2StartByte() const { return StartWord(0x13) * 2u; }
    uint32_t StrideBytes() const { return ((uint32_t(Reg(0x17) & 0x7u) << 8) | Reg(0x16)) * 2u; }
    uint32_t Screen1Pan() const { return Reg(0x18) & PanMask(); }
    uint32_t Screen2Pan() const { return (Reg(0x18) >> 4) & PanMask(); }
    uint32_t LutXrgb(uint32_t index) const { return lut_table_.Xrgb(index, Color()); }
    uint32_t DirectXrgb(uint16_t px, uint32_t bpp) const;
    bool     InkCursor(EpsonInkCursorLayer& layer) const;
    const EpsonDisplayBuffer& Buffer() const { return buffer_; }

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

private:
    static constexpr uint32_t kWindowBytes = 0x400000u;
    static constexpr uint32_t kBufferBase  = 0x200000u;
    static constexpr uint32_t kRegDecode   = 0x3Fu;
    static constexpr uint32_t kRegCount    = 0x32u;

    uint8_t  Reg(uint32_t off) const { return reg_[off]; }
    uint32_t TenBits(uint32_t low) const { return (uint32_t(Reg(low + 1u) & 0x3u) << 8) | Reg(low); }
    uint32_t StartWord(uint32_t low) const {
        return (uint32_t(Reg(low + 2u) & 0xFu) << 16) | (uint32_t(Reg(low + 1u)) << 8) | Reg(low);
    }
    uint32_t PanMask() const;
    uint32_t DisplayLines() const { return TenBits(0x08) + 1u; }
    bool     DualPanel() const { return !Tft() && (Reg(0x02) & 0x02u) != 0u; }
    bool     HostInterfaceDisabled() const { return (Reg(0x1B) & 0x80u) != 0u; }
    bool     SoftwareSuspend() const { return (Reg(0x1A) & 0x01u) != 0u; }
    bool     Swivel() const { return (Reg(0x0D) & 0x80u) != 0u; }
    bool     ScanRunning() const { return (Reg(0x0D) & 0x03u) != 0u && !SoftwareSuspend(); }
    uint8_t  GpioPins() const;

    uint8_t  RegRead(uint32_t addr, uint32_t off);
    void     RegWrite(uint32_t addr, uint32_t off, uint8_t value);
    void     RegStore(uint32_t addr, uint32_t off, uint8_t value);
    void     WriteTiming(uint32_t off, uint8_t value);
    void     CheckOutputFormat() const;
    void     RequireMemoryAccess(uint32_t addr, uint32_t value) const;
    void     RequireOutsideHalfFrameBuffer(uint32_t addr, uint32_t off, uint32_t width) const;
    uint32_t SwivelOffset(uint32_t addr, uint32_t off) const;
    uint32_t MemoryLoad(uint32_t addr, uint32_t off, uint32_t width);
    void     MemoryStore(uint32_t addr, uint32_t off, uint32_t value, uint32_t width);
    void     HardwareReset();
    void     ResetRegisters();
    void     PublishOnPanelEdge();
    void     RetimeScanLocked();

    uint64_t              LineTicks() const;
    GuestCycleClock::Rate PixelRate() const;
    uint8_t               VndStatusBit();

    ScanShape ScanShapeLocked() const override;
    bool      EdgeRaisesInterruptLocked(uint32_t, bool) const override { return false; }
    bool      FrameEdgesInertLocked() const override { return true; }
    void      FrameEdgeLocked(uint32_t) override {}
    void      ScanEdgesRan() override {}

    S1d13505Config*    config_    = nullptr;
    uint32_t           mmio_base_ = 0;
    uint16_t           pins_      = 0;
    uint8_t            reg_[kRegCount] = {};
    EpsonDisplayBuffer buffer_;
    EpsonLutTable      lut_table_;
    EpsonLutPort       lut_port_;
    DisplayModeLatch   mode_latch_;
};

}

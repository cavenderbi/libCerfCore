#pragma once

#include "../../lcd/display_mode_latch.h"
#include "../raster_scan_peripheral.h"
#include "../sh77xx/sh77xx_standby_control.h"

#include <array>
#include <cstdint>

namespace cerfcore {

class EmulatedMemory;
class GuestDeepSleep;
class Sh77xxCpg;
class Sh77xxIntc;
class Sh77xxPortData;
class Sh7707LcdPanel;

class Sh7707Lcdc : public RasterScanPeripheral {
public:
    using RasterScanPeripheral::RasterScanPeripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override { return 0x040000C0u; }
    uint32_t MmioSize() const override { return 0x10u; }

    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    uint8_t  ReadByte(uint32_t addr) override { HaltUnsupportedAccess("SH7707 LCDC ReadByte", addr, 0); }
    uint32_t ReadWord(uint32_t addr) override { HaltUnsupportedAccess("SH7707 LCDC ReadWord", addr, 0); }
    void WriteByte(uint32_t addr, uint8_t v) override { HaltUnsupportedAccess("SH7707 LCDC WriteByte", addr, v); }
    void WriteWord(uint32_t addr, uint32_t v) override { HaltUnsupportedAccess("SH7707 LCDC WriteWord", addr, v); }

    static constexpr uint32_t kModeDual     = 2u;
    static constexpr uint32_t kModeTft      = 4u;
    static constexpr uint32_t kModeStnColor = 5u;

    struct Scanout {
        bool                     panel_lit    = false;
        bool                     data_out     = false;
        bool                     linear_bytes = false;
        bool                     dual         = false;
        uint32_t                 mode         = 0;
        uint32_t                 bits_per_dot = 1;
        uint32_t                 dots         = 0;
        uint32_t                 panel_lines  = 0;
        uint32_t                 upper_pa     = 0;
        uint32_t                 lower_pa     = 0;
        std::array<uint16_t, 16> palette{};

        uint32_t PresentedLines() const { return dual ? panel_lines * 2u : panel_lines; }
    };

    Scanout GetScanout() const;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

protected:
    ScanShape ScanShapeLocked() const override;
    bool      EdgeRaisesInterruptLocked(uint32_t, bool) const override { return vie_; }
    bool      FrameEdgesInertLocked() const override { return !vie_; }
    void      FrameEdgeLocked(uint32_t edge_index) override;
    void      ScanEdgesRan() override;

private:
    static constexpr uint32_t kDisplayRegs = 7;
    static constexpr uint32_t kPaletteRegs = 16;
    static constexpr uint32_t kDmaRegs     = 5;

    uint16_t ReadDisplayLocked(uint32_t addr, uint32_t index) const;
    void     WriteDisplayLocked(uint32_t addr, uint32_t index, uint16_t value);
    void     CheckModeLocked(uint32_t addr, uint16_t ldr1) const;
    void     CheckFetchLocked(uint32_t addr, uint16_t value) const;
    void     CheckTimingLocked(uint32_t addr, uint16_t value) const;
    void     CheckPanelDataLocked(uint32_t addr, uint16_t value, uint16_t low, uint16_t high, uint64_t bytes) const;
    bool     DonPinLocked() const;
    void     DriveDonPin();
    bool     ScanWantedLocked() const;
    void     SyncScanLocked(bool timing_changed);
    void     Publish();
    void     OnModuleStopChange();
    void     ResetRegisters(bool power_on);

    uint16_t ar_ = 0;
    std::array<uint16_t, kDisplayRegs> dr_{};
    bool     vie_ = false;
    bool     vi_  = false;
    std::array<uint16_t, kPaletteRegs> pr_{};
    std::array<uint16_t, kDmaRegs>     dmr_{};

    bool display_stopped_  = false;
    bool transfer_stopped_ = false;
    bool standby_          = false;
    bool lit_              = false;

    DisplayModeLatch latch_;

    Sh77xxCpg*      cpg_    = nullptr;
    Sh77xxIntc*     intc_   = nullptr;
    Sh77xxStbcr2*   stbcr2_ = nullptr;
    Sh7707LcdPanel* panel_  = nullptr;
    EmulatedMemory* mem_    = nullptr;
    GuestDeepSleep* sleep_  = nullptr;
    Sh77xxPortData* ports_  = nullptr;
};

}

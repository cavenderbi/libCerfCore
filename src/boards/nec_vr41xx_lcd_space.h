#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include "../lcd/display_size_latch.h"
#include "nec_vr41xx_lcd_panel.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

class Vr41xxGiu;

class NecVr41xxLcdSpace : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override { return kLcdCsBase; }
    uint32_t MmioSize() const override { return kLcdCsSize; }

    uint8_t  ReadByte (uint32_t addr) override;
    uint16_t ReadHalf (uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t  value) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

    bool           DisplayOn() const;
    uint32_t       GuestW() const      { return panel_->Width(); }
    uint32_t       GuestH() const      { return panel_->Height(); }
    uint32_t       Bpp() const         { return kBpp; }
    uint32_t       StrideBytes() const { return panel_->StrideBytes(); }
    const uint8_t* FbBytes() const     { return fb_.data(); }

protected:
    uint32_t ReadWordAsHalves (uint32_t addr) override;
    void     WriteWordAsHalves(uint32_t addr, uint32_t value) override;

private:
    static constexpr uint32_t kLcdCsBase = 0x0A000000u;
    static constexpr uint32_t kLcdCsSize = 0x00800000u;
    static constexpr uint32_t kBpp       = 2u;

    void PublishScreenSizeOnDisplayOn();

    NecVr41xxLcdPanel*   panel_ = nullptr;
    Vr41xxGiu*           giu_   = nullptr;
    std::vector<uint8_t> fb_;
    DisplaySizeLatch     size_latch_;
};

}

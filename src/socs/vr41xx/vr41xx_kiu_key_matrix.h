#pragma once

#include "vr41xx_kiu_regs.h"

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

class Vr41xxKiuKeyMatrix {
public:
    static constexpr uint32_t kRegs = cerf_vr41xx_kiu_regs::kDataRegs;

    void Configure(bool scanline_none_drives_all, bool scan_pins_drive_piod);
    void SetHeld(const uint16_t (&pins)[kRegs]);
    void ClearHeld();

    uint16_t ScanPins(uint16_t scanline) const;
    uint16_t OutputPins(uint16_t scanline, uint16_t gpen) const;
    uint16_t ScanLevel(uint16_t piod) const;
    uint8_t  Kport(uint16_t scanline, uint16_t gpen, uint16_t piod, uint16_t scan_pins) const;
    bool     LatchLine(uint32_t line, uint16_t scanline, uint16_t gpen, uint16_t piod);

    uint16_t Data(uint32_t reg) const { return data_[reg]; }
    void     ClearData();
    void     SaveData(StateWriter& w) const;
    void     RestoreData(StateReader& r);

private:
    bool     scanline_none_drives_all_ = false;
    bool     scan_pins_drive_piod_     = false;
    uint16_t held_[kRegs]              = {0, 0, 0, 0, 0, 0};
    uint16_t data_[kRegs]              = {0, 0, 0, 0, 0, 0};
};

}

#include "vr41xx_kiu_key_matrix.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

using namespace cerf_vr41xx_kiu_regs;

void Vr41xxKiuKeyMatrix::Configure(bool scanline_none_drives_all, bool scan_pins_drive_piod) {
    scanline_none_drives_all_ = scanline_none_drives_all;
    scan_pins_drive_piod_     = scan_pins_drive_piod;
}

void Vr41xxKiuKeyMatrix::SetHeld(const uint16_t (&pins)[kRegs]) {
    for (uint32_t i = 0; i < kRegs; ++i) held_[i] = pins[i];
}

void Vr41xxKiuKeyMatrix::ClearHeld() {
    for (uint16_t& w : held_) w = 0;
}

uint16_t Vr41xxKiuKeyMatrix::ScanPins(uint16_t scanline) const {
    return static_cast<uint16_t>((1u << ScanLineCount(scanline)) - 1u);
}

uint16_t Vr41xxKiuKeyMatrix::OutputPins(uint16_t scanline, uint16_t gpen) const {
    if (ScanLineCount(scanline) == 0u && scanline_none_drives_all_) return kGpenMask;
    return static_cast<uint16_t>(gpen & ~ScanPins(scanline));
}

uint16_t Vr41xxKiuKeyMatrix::ScanLevel(uint16_t piod) const {
    return scan_pins_drive_piod_ ? piod : kGpenMask;
}

uint8_t Vr41xxKiuKeyMatrix::Kport(uint16_t scanline, uint16_t gpen, uint16_t piod,
                                  uint16_t scan_pins) const {
    const uint16_t high = static_cast<uint16_t>((OutputPins(scanline, gpen) & piod) |
                                                (scan_pins & ScanLevel(piod)));
    uint8_t kport = 0;
    for (uint32_t line = 0; line < 2u * kRegs; ++line) {
        if (((high >> line) & 1u) == 0u) continue;
        kport |= static_cast<uint8_t>(held_[line >> 1] >> (8u * (line & 1u)));
    }
    return kport;
}

bool Vr41xxKiuKeyMatrix::LatchLine(uint32_t line, uint16_t scanline, uint16_t gpen, uint16_t piod) {
    const uint8_t  value = Kport(scanline, gpen, piod, static_cast<uint16_t>(1u << line));
    const uint32_t shift = 8u * (line & 1u);
    uint16_t&      reg   = data_[line >> 1];
    reg = static_cast<uint16_t>((reg & ~(0xFFu << shift)) | (static_cast<uint32_t>(value) << shift));
    return value != 0u;
}

void Vr41xxKiuKeyMatrix::ClearData() {
    for (uint16_t& w : data_) w = 0;
}

void Vr41xxKiuKeyMatrix::SaveData(StateWriter& w) const {
    for (uint16_t v : data_) w.Write("matrix", v);
}

void Vr41xxKiuKeyMatrix::RestoreData(StateReader& r) {
    for (uint16_t& v : data_) r.Read("matrix", v);
}

}

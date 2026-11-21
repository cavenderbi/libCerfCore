#include "sed1356_lut.h"

namespace cerfcore {

uint8_t Sed1356Lut::Read(uint32_t select) {
    return port_.Read(select == 0x2u ? crt_ : lcd_);
}

void Sed1356Lut::Write(uint32_t select, uint8_t value) {
    EpsonLutTable* const both[]     = {&lcd_, &crt_};
    EpsonLutTable* const lcd_only[] = {&lcd_};
    EpsonLutTable* const crt_only[] = {&crt_};
    switch (select) {
        case 0x0u: port_.Write(value, both); return;
        case 0x1u: port_.Write(value, lcd_only); return;
        case 0x2u: port_.Write(value, crt_only); return;
        default:   port_.Write(value, {}); return;
    }
}

void Sed1356Lut::SaveState(StateWriter& w) const {
    lcd_.SaveState(w, "lcd_lut");
    crt_.SaveState(w, "crt_lut");
    port_.SaveState(w);
}

void Sed1356Lut::RestoreState(StateReader& r) {
    lcd_.RestoreState(r, "lcd_lut");
    crt_.RestoreState(r, "crt_lut");
    port_.RestoreState(r);
}

}

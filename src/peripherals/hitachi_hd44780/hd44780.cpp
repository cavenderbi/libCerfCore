#include <cerfcore/peripherals/peripheral_base.h>

#include "../../core/board_parts.h"
#include "hitachi_hd44780_part_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../peripheral_dispatcher.h"

#include <cstdint>
#include <cstring>

namespace cerfcore {

namespace {

constexpr uint32_t kBase           = 0x1A020000u;
constexpr uint32_t kSize           = 2u;
constexpr uint32_t kOffInstruction = 0u;

constexpr uint8_t kSetDdram    = 0x80u;
constexpr uint8_t kSetCgram    = 0x40u;
constexpr uint8_t kFunctionSet = 0x20u;
constexpr uint8_t kShift       = 0x10u;
constexpr uint8_t kDisplay     = 0x08u;
constexpr uint8_t kEntryMode   = 0x04u;
constexpr uint8_t kReturnHome  = 0x02u;
constexpr uint8_t kClear       = 0x01u;

constexpr uint8_t kFuncAllFields = 0x1Fu;
constexpr uint8_t kFuncEightBit = 0x10u;
constexpr uint8_t kFuncTwoLines = 0x08u;
constexpr uint8_t kFuncFont5x10 = 0x04u;
constexpr uint8_t kShiftDisplay = 0x08u;
constexpr uint8_t kShiftRight   = 0x04u;
constexpr uint8_t kDisplayOn    = 0x04u;
constexpr uint8_t kCursorOn     = 0x02u;
constexpr uint8_t kBlinkOn      = 0x01u;
constexpr uint8_t kEntryInc     = 0x02u;
constexpr uint8_t kEntryShift   = 0x01u;

constexpr uint8_t kDdramAddrMask = 0x7Fu;
constexpr uint8_t kCgramAddrMask = 0x3Fu;
constexpr uint8_t kOneLineLast   = 0x4Fu;
constexpr uint8_t kLine1Last     = 0x27u;
constexpr uint8_t kLine2First    = 0x40u;
constexpr uint8_t kLine2Last     = 0x67u;
constexpr uint8_t kBlank         = 0x20u;

constexpr int32_t kOneLineLength = 80;
constexpr int32_t kTwoLineLength = 40;

class Hd44780 : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardParts>().Has(PartId::HitachiHd44780);
    }

    void OnReady() override {
        PowerOnReset();
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
            if (kind == ResetLineKind::Rtc) PowerOnReset();
        });
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    uint8_t ReadByte(uint32_t addr) override {
        if (addr - kBase != kOffInstruction) {
            emu_.Get<Fatal>().Die("HD44780: data register read (AC 0x%02X, %s)", ac_,
                                  cgram_selected_ ? "CGRAM" : "DDRAM");
        }
        return ac_;
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        if (addr - kBase == kOffInstruction) Instruction(value);
        else                                 WriteData(value);
    }

    void SaveState(StateWriter& w) override {
        w.WriteBytes("hd44780_ddram", ddram_, sizeof(ddram_));
        w.WriteBytes("hd44780_cgram", cgram_, sizeof(cgram_));
        w.Write("hd44780_ac", ac_);
        w.Write("hd44780_cgram_selected", cgram_selected_);
        w.Write("hd44780_increment", increment_);
        w.Write("hd44780_entry_shift", entry_shift_);
        w.Write("hd44780_display_on", display_on_);
        w.Write("hd44780_cursor_on", cursor_on_);
        w.Write("hd44780_blink_on", blink_on_);
        w.Write("hd44780_two_lines", two_lines_);
        w.Write("hd44780_font_5x10", font_5x10_);
        w.Write("hd44780_display_shift", display_shift_);
        w.Write("hd44780_past_head", past_head_);
    }

    void RestoreState(StateReader& r) override {
        r.ReadBytes("hd44780_ddram", ddram_, sizeof(ddram_));
        r.ReadBytes("hd44780_cgram", cgram_, sizeof(cgram_));
        r.Read("hd44780_ac", ac_);
        r.Read("hd44780_cgram_selected", cgram_selected_);
        r.Read("hd44780_increment", increment_);
        r.Read("hd44780_entry_shift", entry_shift_);
        r.Read("hd44780_display_on", display_on_);
        r.Read("hd44780_cursor_on", cursor_on_);
        r.Read("hd44780_blink_on", blink_on_);
        r.Read("hd44780_two_lines", two_lines_);
        r.Read("hd44780_font_5x10", font_5x10_);
        r.Read("hd44780_display_shift", display_shift_);
        r.Read("hd44780_past_head", past_head_);
    }

private:
    void PowerOnReset() {
        Clear();
        two_lines_         = false;
        font_5x10_         = false;
        display_on_        = false;
        cursor_on_         = false;
        blink_on_          = false;
        entry_shift_       = false;
        past_head_         = false;
        std::memset(cgram_, 0, sizeof(cgram_));
    }

    void Clear() {
        std::memset(ddram_, kBlank, sizeof(ddram_));
        ac_             = 0u;
        cgram_selected_ = false;
        increment_      = true;
        display_shift_  = 0;
    }

    void Instruction(uint8_t v) {
        if ((v & ~kFuncAllFields) != kFunctionSet) past_head_ = true;
        if ((v & kSetDdram) != 0u)         SetDdramAddress(v & kDdramAddrMask);
        else if ((v & kSetCgram) != 0u)    SetCgramAddress(v & kCgramAddrMask);
        else if ((v & kFunctionSet) != 0u) FunctionSet(v);
        else if ((v & kShift) != 0u)       Shift(v);
        else if ((v & kDisplay) != 0u)     DisplayControl(v);
        else if ((v & kEntryMode) != 0u)   EntryMode(v);
        else if ((v & kReturnHome) != 0u)  ReturnHome();
        else if (v == kClear)              Clear();
        else emu_.Get<Fatal>().Die("HD44780: instruction 0x%02X", v);
    }

    void SetDdramAddress(uint8_t addr) {
        const bool valid = two_lines_ ? (addr <= kLine1Last || (addr >= kLine2First && addr <= kLine2Last))
                                      : addr <= kOneLineLast;
        if (!valid) {
            emu_.Get<Fatal>().Die("HD44780: set DDRAM address 0x%02X outside the %s map", addr,
                                  two_lines_ ? "2-line" : "1-line");
        }
        ac_             = addr;
        cgram_selected_ = false;
    }

    void SetCgramAddress(uint8_t addr) {
        ac_             = addr;
        cgram_selected_ = true;
    }

    void FunctionSet(uint8_t v) {
        if ((v & kFuncEightBit) == 0u) {
            emu_.Get<Fatal>().Die("HD44780: function set 0x%02X selects the 4-bit interface", v);
        }
        const bool two_lines = (v & kFuncTwoLines) != 0u;
        const bool font_5x10 = !two_lines && (v & kFuncFont5x10) != 0u;
        if (past_head_ && (two_lines != two_lines_ || font_5x10 != font_5x10_)) {
            emu_.Get<Fatal>().Die("HD44780: function set 0x%02X changes N/F after other instructions", v);
        }
        two_lines_ = two_lines;
        font_5x10_ = font_5x10;
    }

    void Shift(uint8_t v) {
        const bool right = (v & kShiftRight) != 0u;
        if ((v & kShiftDisplay) != 0u) ShiftDisplay(right);
        else                           StepAddress(right);
    }

    void DisplayControl(uint8_t v) {
        display_on_ = (v & kDisplayOn) != 0u;
        cursor_on_  = (v & kCursorOn) != 0u;
        blink_on_   = (v & kBlinkOn) != 0u;
    }

    void EntryMode(uint8_t v) {
        increment_   = (v & kEntryInc) != 0u;
        entry_shift_ = (v & kEntryShift) != 0u;
    }

    void ReturnHome() {
        ac_             = 0u;
        cgram_selected_ = false;
        display_shift_  = 0;
    }

    void WriteData(uint8_t v) {
        past_head_ = true;
        if (cgram_selected_) {
            cgram_[ac_] = v;
            StepAddress(increment_);
            return;
        }
        ddram_[ac_] = v;
        StepAddress(increment_);
        if (entry_shift_) ShiftDisplay(!increment_);
    }

    void StepAddress(bool up) {
        if (cgram_selected_) {
            if (up ? ac_ == kCgramAddrMask : ac_ == 0u) WrapHalt(up);
            ac_ = static_cast<uint8_t>(up ? ac_ + 1u : ac_ - 1u);
            return;
        }
        if (up) {
            if (two_lines_ && ac_ == kLine1Last) {
                ac_ = kLine2First;
                return;
            }
            if (ac_ == (two_lines_ ? kLine2Last : kOneLineLast)) WrapHalt(up);
            ++ac_;
            return;
        }
        if (ac_ == 0u || (two_lines_ && ac_ == kLine2First)) WrapHalt(up);
        --ac_;
    }

    void ShiftDisplay(bool right) {
        const int32_t length = two_lines_ ? kTwoLineLength : kOneLineLength;
        display_shift_       = (display_shift_ + (right ? 1 : length - 1)) % length;
    }

    [[noreturn]] void WrapHalt(bool up) {
        emu_.Get<Fatal>().Die("HD44780: %s %s address 0x%02X past the end of the %s map",
                              up ? "increment of" : "decrement of",
                              cgram_selected_ ? "CGRAM" : "DDRAM", ac_,
                              two_lines_ ? "2-line" : "1-line");
    }

    uint8_t ddram_[0x80]       = {};
    uint8_t cgram_[0x40]       = {};
    uint8_t ac_                = 0u;
    bool    cgram_selected_    = false;
    bool    increment_         = true;
    bool    entry_shift_       = false;
    bool    display_on_        = false;
    bool    cursor_on_         = false;
    bool    blink_on_          = false;
    bool    two_lines_         = false;
    bool    font_5x10_         = false;
    int32_t display_shift_     = 0;
    bool    past_head_         = false;
};

}

REGISTER_SERVICE(Hd44780);

}

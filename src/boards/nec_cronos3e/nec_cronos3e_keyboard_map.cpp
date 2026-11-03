#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include "../nec_vr41xx_keyboard_input.h"
#include "nec_mobilepro_780_id.h"
#include "nec_mobilepro_790_id.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr uint8_t kFnLayer  = 1;
constexpr uint8_t kAltLayer = 2;

const std::vector<KeyBinding> kBindings = {
    { 0x20,  0, nullptr, 0, 0 }, { 0xDD,  1, nullptr, 0, 0 }, { 0xDC,  2, nullptr, 0, 0 },
    { 0xBF,  3, nullptr, 0, 0 }, { 0x25,  4, nullptr, 0, 0 }, { 0x27,  5, nullptr, 0, 0 },
    { 0x0D,  6, nullptr, 0, 0 }, { 0x4C,  7, nullptr, 0, 0 }, { 0xDB,  9, nullptr, 0, 0 },
    { 0xDE, 10, nullptr, 0, 0 }, { 0xBA, 11, nullptr, 0, 0 }, { 0x26, 12, nullptr, 0, 0 },
    { 0x28, 13, nullptr, 0, 0 }, { 0xBE, 14, nullptr, 0, 0 }, { 0x4F, 15, nullptr, 0, 0 },
    { 0x5B, 19, nullptr, 0, 0 }, { 0x56, 20, nullptr, 0, 0 }, { 0x43, 21, nullptr, 0, 0 },
    { 0x58, 22, nullptr, 0, 0 }, { 0x5A, 23, nullptr, 0, 0 }, { 0xBB, 25, nullptr, 0, 0 },
    { 0xBD, 26, nullptr, 0, 0 }, { 0xC0, 27, nullptr, 0, 0 }, { 0x46, 28, nullptr, 0, 0 },
    { 0x44, 29, nullptr, 0, 0 }, { 0x53, 30, nullptr, 0, 0 }, { 0x41, 31, nullptr, 0, 0 },
    { 0x38, 32, nullptr, 0, 0 }, { 0x37, 33, nullptr, 0, 0 }, { 0x36, 34, nullptr, 0, 0 },
    { 0x35, 35, nullptr, 0, 0 }, { 0x52, 36, nullptr, 0, 0 }, { 0x45, 37, nullptr, 0, 0 },
    { 0x57, 38, nullptr, 0, 0 }, { 0x51, 39, nullptr, 0, 0 }, { 0xBC, 40, nullptr, 0, 0 },
    { 0x4D, 41, nullptr, 0, 0 }, { 0x4E, 42, nullptr, 0, 0 }, { 0x42, 43, nullptr, 0, 0 },
    { 0x30, 46, nullptr, 0, 0 }, { 0x39, 47, nullptr, 0, 0 }, { 0x4B, 48, nullptr, 0, 0 },
    { 0x4A, 49, nullptr, 0, 0 }, { 0x48, 50, nullptr, 0, 0 }, { 0x47, 51, nullptr, 0, 0 },
    { 0x34, 52, nullptr, 0, 0 }, { 0x33, 53, nullptr, 0, 0 }, { 0x32, 54, nullptr, 0, 0 },
    { 0x31, 55, nullptr, 0, 0 }, { 0x49, 56, nullptr, 0, 0 }, { 0x55, 57, nullptr, 0, 0 },
    { 0x59, 58, nullptr, 0, 0 }, { 0x54, 59, nullptr, 0, 0 }, { 0x14, 61, nullptr, 0, 0 },
    { 0x2E, 62, nullptr, 0, 0 }, { 0x1B, 63, nullptr, 0, 0 }, { 0x08, 68, nullptr, 0, 0 },
    { 0x50, 69, nullptr, 0, 0 }, { 0x09, 70, nullptr, 0, 0 }, { 0x79, 78, nullptr, 0, 0 },
    { 0x78, 79, nullptr, 0, 0 }, { 0x77, 84, nullptr, 0, 0 }, { 0x76, 85, nullptr, 0, 0 },
    { 0x75, 86, nullptr, 0, 0 }, { 0x74, 87, nullptr, 0, 0 }, { 0x10, 91, nullptr, 0, 0 },
    { 0x73, 92, nullptr, 0, 0 }, { 0x72, 93, nullptr, 0, 0 }, { 0x71, 94, nullptr, 0, 0 },
    { 0x70, 95, nullptr, 0, 0 }, { 0x11, 82, nullptr, 0, 0 },
    { 0x7B, 71, "Fn",  0, kFnLayer },
    { 0x12, 73, "Alt", 0, kAltLayer },
    { 0xA5, 64, "Alt", 0, kAltLayer },
    { 0xDF, kNecVr41xxPowerSwitchCode, "Power", 0, 0 },
    { 0x7A, kNecVr41xxRecordButtonCode, "Record", 0, 0 },
    { 0x10, 0, "Num Lock",  kFnLayer, 0 },
    { 0x20, 0, "Backlight", kFnLayer, 0 },
    { 0x34, 0, "£",         kFnLayer, 0 },
    { 0x35, 0, "€",         kFnLayer, 0 },
    { 0x78, 0, "Rotate",    kFnLayer, 0 },
    { 0x26, 0, "PgUp", kAltLayer, 0 },
    { 0x25, 0, "Home", kAltLayer, 0 },
    { 0x28, 0, "PgDn", kAltLayer, 0 },
    { 0x27, 0, "End",  kAltLayer, 0 },
    { 0xBC, 0, "Contrast -", kAltLayer, 0 },
    { 0xBE, 0, "Contrast +", kAltLayer, 0 },
};

class NecCronos3eKeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    bool ShouldRegister() override {
        const auto board = emu_.Get<BoardContext>().GetBoardId();
        return board == BoardId::NecMobilepro790 || board == BoardId::NecMobilepro780;
    }

    const std::vector<KeyBinding>& Bindings() const override { return kBindings; }
};

}

REGISTER_SERVICE_AS(NecCronos3eKeyboardMap, KeyboardMap);

}

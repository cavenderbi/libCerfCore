#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include "philips_nino_keypad_codes.h"
#include <cerfcore/boards/board_context.h>
#include "philips_nino_family_id.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr char kOkLabel[]   = "OK";
constexpr char kBackLabel[] = "Back";
constexpr char kApp1Label[] = "App1";
constexpr char kApp2Label[] = "App2";
constexpr char kApp3Label[] = "App3";
constexpr char kApp4Label[] = "App4";

/* Button source pins are keybddr.dll's keymap dword_18C4048 (scan sub_18C16CC):
   MFIO 18/31/16 = Action/Back/Up, I/O pins 0/6/2/3/4 = Down/App1..App4. */
const std::vector<KeyBinding> kBindings = {
    { 0x0D, 18,                        kOkLabel,   0, 0 },
    { 0x27, 18,                        kOkLabel,   0, 0 },
    { 0x1B, 31,                        kBackLabel, 0, 0 },
    { 0x25, 31,                        kBackLabel, 0, 0 },
    { 0x26, 16,                        nullptr, 0, 0 },   /* Up    */
    { 0x28, kNinoKeypadIoPinFlag | 0u, nullptr, 0, 0 },   /* Down  */
    { 0x70, kNinoKeypadIoPinFlag | 6u, kApp1Label, 0, 0 },
    { 0x71, kNinoKeypadIoPinFlag | 2u, kApp2Label, 0, 0 },
    { 0x72, kNinoKeypadIoPinFlag | 3u, kApp3Label, 0, 0 },
    { 0x73, kNinoKeypadIoPinFlag | 4u, kApp4Label, 0, 0 },
};

class PhilipsNinoKeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::PhilipsNino;
    }

    const std::vector<KeyBinding>& Bindings() const override { return kBindings; }
};

}  /* namespace */

REGISTER_SERVICE_AS(PhilipsNinoKeyboardMap, KeyboardMap);

}

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/boards/board_context.h>
#include "siemens_simpad_family_id.h"
#include "siemens_simpad_power_button.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr char kAltLabel[]       = "Alt";
constexpr char kUpArrowLabel[]   = "↑";
constexpr char kDownArrowLabel[] = "↓";
constexpr char kEscLabel[]       = "Esc";
constexpr char kEnterLabel[]     = "Enter";
constexpr char kPowerLabel[]     = "Power";

const std::vector<KeyBinding> kBindings = {
    { 0x70, 0, nullptr,  0, 0 },   /* F1     -> PROG1 (guest F1)        */
    { 0x12, 1, nullptr,  0, 0 },   /* Alt    -> PROG2 (guest Alt)       */
    { 0x71, 1, kAltLabel, 0, 0 },
    { 0x26, 2, nullptr,  0, 0 },   /* Up     -> UP    (guest Up)        */
    { 0x21, 2, kUpArrowLabel, 0, 0 },
    { 0x28, 3, nullptr,  0, 0 },   /* Down   -> DOWN  (guest Down)      */
    { 0x22, 3, kDownArrowLabel, 0, 0 },
    { 0x25, 4, kEscLabel, 0, 0 },
    { 0x1B, 4, nullptr,  0, 0 },   /* Esc    -> LEFT  (guest Esc/Back)  */
    { 0x27, 5, kEnterLabel, 0, 0 },
    { 0x0D, 5, nullptr,  0, 0 },   /* Enter  -> RIGHT (guest Enter/OK)  */
    { 0xDF, kSimpadPowerButtonCode, kPowerLabel, 0, 0 },
};

class SiemensSimpadKeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::SiemensSimpad;
    }

    const std::vector<KeyBinding>& Bindings() const override { return kBindings; }
};

}  /* namespace */

REGISTER_SERVICE_AS(SiemensSimpadKeyboardMap, KeyboardMap);

}

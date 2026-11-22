#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/host/virtual_keys.h>
#include "hp_jornada_540_buttons.h"
#include "hp_jornada_540_family_id.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr uint32_t Code(HpJornada540Button button) { return static_cast<uint32_t>(button); }

constexpr char kQuickLaunch1Label[] = "QL1";
constexpr char kQuickLaunch2Label[] = "QL2";
constexpr char kQuickLaunch3Label[] = "QL3";
constexpr char kQuickLaunch4Label[] = "QL4";
constexpr char kRecordLabel[]       = "Record";
constexpr char kOnOffLabel[]        = "On/Off";

const std::vector<KeyBinding> kBindings = {
    {kVkUp, Code(HpJornada540Button::kUp), nullptr, 0, 0},
    {kVkDown, Code(HpJornada540Button::kDown), nullptr, 0, 0},
    {kVkReturn, Code(HpJornada540Button::kAction), nullptr, 0, 0},
    {kVkF1, Code(HpJornada540Button::kQuickLaunch1), kQuickLaunch1Label, 0, 0},
    {kVkF2, Code(HpJornada540Button::kQuickLaunch2), kQuickLaunch2Label, 0, 0},
    {kVkF3, Code(HpJornada540Button::kQuickLaunch3), kQuickLaunch3Label, 0, 0},
    {kVkF4, Code(HpJornada540Button::kQuickLaunch4), kQuickLaunch4Label, 0, 0},
    {kVkF5, Code(HpJornada540Button::kRecord), kRecordLabel, 0, 0},
    {kVkOff, Code(HpJornada540Button::kOnOff), kOnOffLabel, 0, 0},
};

class HpJornada540KeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

    const std::vector<KeyBinding>& Bindings() const override { return kBindings; }
};

}

REGISTER_SERVICE_AS(HpJornada540KeyboardMap, KeyboardMap);

}

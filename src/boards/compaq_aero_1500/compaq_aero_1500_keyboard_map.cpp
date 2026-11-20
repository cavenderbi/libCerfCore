#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include "compaq_aero_1500_family_id.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr char kActionLabel[] = "Action";
constexpr char kExitLabel[]   = "Exit";
constexpr char kApp1Label[]   = "App1";
constexpr char kApp2Label[]   = "App2";
constexpr char kApp3Label[]   = "App3";
constexpr char kApp4Label[]   = "App4";
constexpr char kRecordLabel[] = "Record";
constexpr char kPowerLabel[]  = "Power";

const std::vector<KeyBinding> kBindings = {
    { 0x0D, 18, kActionLabel, 0, 0 },
    { 0x1B,  7, kExitLabel,   0, 0 },
    { 0x26, 16, nullptr,      0, 0 },
    { 0x28, 17, nullptr,      0, 0 },
    { 0x70, 12, kApp1Label,   0, 0 },
    { 0x71, 11, kApp2Label,   0, 0 },
    { 0x72, 10, kApp3Label,   0, 0 },
    { 0x73,  9, kApp4Label,   0, 0 },
    { 0x74,  1, kRecordLabel, 0, 0 },
    { 0xDF,  8, kPowerLabel,  0, 0 },
};

class CompaqAero1500KeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CompaqAero1500;
    }

    const std::vector<KeyBinding>& Bindings() const override { return kBindings; }
};

}

REGISTER_SERVICE_AS(CompaqAero1500KeyboardMap, KeyboardMap);

}

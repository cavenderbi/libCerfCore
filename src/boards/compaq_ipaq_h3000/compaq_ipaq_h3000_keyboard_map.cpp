#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/boards/board_context.h>
#include "compaq_ipaq_h3000_button_codes.h"
#include "compaq_ipaq_h3000_family_id.h"
#include "compaq_ipaq_h3000_gpio_buttons.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr uint32_t kMicroRecord   = 1;
constexpr uint32_t kMicroCalendar = 2;
constexpr uint32_t kMicroContacts = 3;
constexpr uint32_t kMicroQ        = 4;
constexpr uint32_t kMicroStart    = 5;
constexpr uint32_t kMicroUp       = 6;
constexpr uint32_t kMicroRight    = 7;
constexpr uint32_t kMicroLeft     = 8;
constexpr uint32_t kMicroDown     = 9;
constexpr uint32_t kGpioAction    = kIpaqH3000GpioButtonFlag | kIpaqH3000ActionButtonPin;
constexpr uint32_t kGpioPower     = kIpaqH3000GpioButtonFlag | kIpaqH3000PowerButtonPin;

constexpr char kApp1Label[]   = "App1";
constexpr char kApp2Label[]   = "App2";
constexpr char kApp3Label[]   = "App3";
constexpr char kApp4Label[]   = "App4";
constexpr char kRecordLabel[] = "Record";
constexpr char kPowerLabel[]  = "Power";

const std::vector<KeyBinding> kBindings = {
    { 0x0D, kGpioAction,    nullptr,  0, 0 },
    { 0x26, kMicroUp,       nullptr,  0, 0 },
    { 0x27, kMicroRight,    nullptr,  0, 0 },
    { 0x25, kMicroLeft,     nullptr,  0, 0 },
    { 0x28, kMicroDown,     nullptr,  0, 0 },
    { 0x70, kMicroCalendar, kApp1Label,   0, 0 },
    { 0x71, kMicroContacts, kApp2Label,   0, 0 },
    { 0x72, kMicroQ,        kApp3Label,   0, 0 },
    { 0x73, kMicroStart,    kApp4Label,   0, 0 },
    { 0x74, kMicroRecord,   kRecordLabel, 0, 0 },
    { 0xDF, kGpioPower,     kPowerLabel,  0, 0 },
};

class CompaqIpaqH3000KeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::CompaqIpaqH3000;
    }

    const std::vector<KeyBinding>& Bindings() const override { return kBindings; }
};

}

REGISTER_SERVICE_AS(CompaqIpaqH3000KeyboardMap, KeyboardMap);

}

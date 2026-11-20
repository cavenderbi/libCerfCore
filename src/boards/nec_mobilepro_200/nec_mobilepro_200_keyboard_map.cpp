#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/boards/board_context.h>
#include "../nec_vr41xx_keyboard_input.h"
#include "nec_mobilepro_200_family_id.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr uint8_t kAltLayer = 1u;

constexpr char kAltLabel[]          = "Alt";
constexpr char kPowerLabel[]        = "Power";
constexpr char kPageUpLabel[]       = "PgUp";
constexpr char kHomeLabel[]         = "Home";
constexpr char kPageDownLabel[]     = "PgDn";
constexpr char kEndLabel[]          = "End";
constexpr char kContrastDownLabel[] = "Contrast -";
constexpr char kContrastUpLabel[]   = "Contrast +";

class NecMobilepro200KeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro200;
    }

    const std::vector<KeyBinding>& Bindings() const override {
        static const std::vector<KeyBinding> kBindings = {
            {0x10u, 0u, nullptr, 0u, 0u},
            {0xA0u, 0u, nullptr, 0u, 0u},
            {0xA1u, 56u, nullptr, 0u, 0u},
            {0x09u, 1u, nullptr, 0u, 0u},
            {0xC0u, 2u, nullptr, 0u, 0u},
            {0x51u, 3u, nullptr, 0u, 0u},
            {0x1Bu, 4u, nullptr, 0u, 0u},
            {0x31u, 5u, nullptr, 0u, 0u},
            {0x5Bu, 6u, nullptr, 0u, 0u},
            {0x71u, 7u, nullptr, 0u, 0u},
            {0x11u, 8u, nullptr, 0u, 0u},
            {0xA2u, 8u, nullptr, 0u, 0u},
            {0xA3u, 8u, nullptr, 0u, 0u},
            {0x5Au, 9u, nullptr, 0u, 0u},
            {0x58u, 10u, nullptr, 0u, 0u},
            {0x41u, 11u, nullptr, 0u, 0u},
            {0x53u, 12u, nullptr, 0u, 0u},
            {0x57u, 13u, nullptr, 0u, 0u},
            {0x45u, 14u, nullptr, 0u, 0u},
            {0x32u, 15u, nullptr, 0u, 0u},
            {0x12u, 16u, kAltLabel, 0u, kAltLayer},
            {0xA4u, 16u, kAltLabel, 0u, kAltLayer},
            {0xA5u, 16u, kAltLabel, 0u, kAltLayer},
            {0x20u, 17u, nullptr, 0u, 0u},
            {0x43u, 18u, nullptr, 0u, 0u},
            {0x56u, 19u, nullptr, 0u, 0u},
            {0x44u, 20u, nullptr, 0u, 0u},
            {0x46u, 21u, nullptr, 0u, 0u},
            {0x52u, 22u, nullptr, 0u, 0u},
            {0x33u, 23u, nullptr, 0u, 0u},
            {0x42u, 24u, nullptr, 0u, 0u},
            {0x4Eu, 25u, nullptr, 0u, 0u},
            {0x47u, 26u, nullptr, 0u, 0u},
            {0x48u, 27u, nullptr, 0u, 0u},
            {0x54u, 28u, nullptr, 0u, 0u},
            {0x59u, 29u, nullptr, 0u, 0u},
            {0x34u, 30u, nullptr, 0u, 0u},
            {0x35u, 31u, nullptr, 0u, 0u},
            {0x4Du, 32u, nullptr, 0u, 0u},
            {0xBCu, 33u, nullptr, 0u, 0u},
            {0x4Au, 34u, nullptr, 0u, 0u},
            {0x4Bu, 35u, nullptr, 0u, 0u},
            {0x55u, 36u, nullptr, 0u, 0u},
            {0x49u, 37u, nullptr, 0u, 0u},
            {0x36u, 38u, nullptr, 0u, 0u},
            {0x37u, 39u, nullptr, 0u, 0u},
            {0x4Cu, 42u, nullptr, 0u, 0u},
            {0x4Fu, 43u, nullptr, 0u, 0u},
            {0x50u, 44u, nullptr, 0u, 0u},
            {0x38u, 45u, nullptr, 0u, 0u},
            {0x39u, 46u, nullptr, 0u, 0u},
            {0x30u, 47u, nullptr, 0u, 0u},
            {0xDBu, 48u, nullptr, 0u, 0u},
            {0xDDu, 49u, nullptr, 0u, 0u},
            {0x25u, 50u, nullptr, 0u, 0u},
            {0xBEu, 51u, nullptr, 0u, 0u},
            {0xBFu, 52u, nullptr, 0u, 0u},
            {0xBAu, 53u, nullptr, 0u, 0u},
            {0xBBu, 54u, nullptr, 0u, 0u},
            {0xBDu, 55u, nullptr, 0u, 0u},
            {0x27u, 57u, nullptr, 0u, 0u},
            {0x26u, 58u, nullptr, 0u, 0u},
            {0x28u, 59u, nullptr, 0u, 0u},
            {0xDEu, 60u, nullptr, 0u, 0u},
            {0x0Du, 61u, nullptr, 0u, 0u},
            {0xDCu, 62u, nullptr, 0u, 0u},
            {0x08u, 63u, nullptr, 0u, 0u},
            {0xDFu, kNecVr41xxPowerSwitchCode, kPowerLabel, 0u, 0u},
            {0x26u, 0u, kPageUpLabel, kAltLayer, 0u},
            {0x25u, 0u, kHomeLabel, kAltLayer, 0u},
            {0x28u, 0u, kPageDownLabel, kAltLayer, 0u},
            {0x27u, 0u, kEndLabel, kAltLayer, 0u},
            {0xBCu, 0u, kContrastDownLabel, kAltLayer, 0u},
            {0xBEu, 0u, kContrastUpLabel, kAltLayer, 0u},
        };
        return kBindings;
    }
};

}

REGISTER_SERVICE_AS(NecMobilepro200KeyboardMap, KeyboardMap);

}

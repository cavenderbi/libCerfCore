#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/boards/board_context.h>
#include "casio_jx670_family_id.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr char kLabelUp[]       = "Up";
constexpr char kLabelDown[]     = "Down";
constexpr char kLabelAction[]   = "Action";
constexpr char kLabelRecord[]   = "Record";
constexpr char kLabelMenu[]     = "Menu";
constexpr char kLabelCalendar[] = "Calendar";
constexpr char kLabelContacts[] = "Contacts";
constexpr char kLabelPower[]    = "Power";

const std::vector<KeyBinding> kBindings = {
    { 0x21,  0, kLabelUp,       0, 0 },
    { 0x22,  1, kLabelDown,     0, 0 },
    { 0x0D,  2, kLabelAction,   0, 0 },
    { 0x26,  4, nullptr,        0, 0 },
    { 0x28,  5, nullptr,        0, 0 },
    { 0x27,  6, nullptr,        0, 0 },
    { 0x25,  7, nullptr,        0, 0 },
    { 0x73,  9, kLabelRecord,   0, 0 },
    { 0x70, 12, kLabelMenu,     0, 0 },
    { 0x71, 11, kLabelCalendar, 0, 0 },
    { 0x72, 10, kLabelContacts, 0, 0 },
    { 0xDF, 15, kLabelPower,    0, 0 },
};

class CasioJx670KeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioJx670;
    }

    const std::vector<KeyBinding>& Bindings() const override { return kBindings; }
};

}

REGISTER_SERVICE_AS(CasioJx670KeyboardMap, KeyboardMap);

}

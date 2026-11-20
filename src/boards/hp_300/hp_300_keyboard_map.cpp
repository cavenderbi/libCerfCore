#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/virtual_keys.h>
#include "../jornada/jornada_keyboard_layers.h"
#include "../matrix_keyboard_map.h"
#include "../sh77xx_port_key_matrix.h"
#include "hp_300_family_id.h"
#include "hp_300_member_straps.h"

#include <array>
#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint8_t kVkBacklight = kVkF9;
constexpr uint8_t kVkFn        = kVkF10;

constexpr char kBacklightLabel[] = "Backlight";
constexpr char kFnLabel[]        = "Fn";

constexpr uint8_t kFnLayerId  = 1u;
constexpr uint8_t kAltLayerId = 2u;

constexpr jornada_kbd::LayerKey kContrastKeys[] = {
    {0xBCu, "Contrast -"},
    {0xBEu, "Contrast +"},
};

constexpr jornada_kbd::LayerKey kFnOnlyKeys[] = {
    {kVkSpace, "Off"},
    {kVkControl, "Num Lock"},
};

constexpr std::array<uint8_t, Sh77xxPortKeyMatrix::kCells> kCellVk = {
    0xA0u, 0x00u, 0x00u, 0x00u, 0x00u, 0x35u, 0x36u, 0x38u, 0x39u, 0xBDu, 0x08u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0xC0u, 0x1Bu, 0x51u, 0x32u, 0x33u, 0x34u, 0x54u, 0x37u, 0x30u, 0xDDu, 0xDCu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x41u, 0x09u, 0x31u, 0x57u, 0x45u, 0x52u, 0x59u, 0x55u, 0x50u, 0xDBu, 0x0Du, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x46u, 0x47u, 0x49u, 0x4Fu, 0x27u, 0xA1u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0xA2u, 0x00u, 0x00u, 0x53u, 0x44u, 0x48u, 0x4Au, 0x4Bu, 0xDEu, 0xBBu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x5Bu, 0x00u, 0x5Au, 0x56u, 0x4Du, 0xBCu, 0x4Cu, 0xBAu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0xA4u, 0x43u, 0x42u, 0x4Eu, 0xBEu, 0xBFu, 0x26u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x20u, 0x00u, 0x58u, 0x78u, 0x00u, 0x79u, 0x2Eu, 0x25u, 0x28u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

class Hp300KeyboardMap : public MatrixKeyboardMap {
public:
    using MatrixKeyboardMap::MatrixKeyboardMap;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override {
        const Hp300MemberStraps& straps    = emu_.Get<Hp300MemberStraps>();
        const bool               backlight = !(straps.Luke() && !straps.Model2());
        for (uint32_t cell = 0; cell < Sh77xxPortKeyMatrix::kCells; ++cell) {
            const uint8_t vk = kCellVk[cell];
            if (vk == 0u) continue;
            if (vk == kVkBacklight) {
                if (backlight) AddCell(vk, cell, kBacklightLabel, 0u);
                continue;
            }
            if (vk == kVkFn) {
                AddCell(vk, cell, kFnLabel, kFnLayerId);
                continue;
            }
            AddCell(vk, cell, nullptr, vk == kVkLMenu ? kAltLayerId : uint8_t{0u});
        }
        for (uint8_t layer : {kFnLayerId, kAltLayerId}) {
            for (const jornada_kbd::LayerKey& k : jornada_kbd::kAltLayer) AddLayerLabel(k.vk, k.label, layer);
            for (const jornada_kbd::LayerKey& k : kContrastKeys) AddLayerLabel(k.vk, k.label, layer);
        }
        for (const jornada_kbd::LayerKey& k : kFnOnlyKeys) AddLayerLabel(k.vk, k.label, kFnLayerId);
    }
};

}

REGISTER_SERVICE_AS(Hp300KeyboardMap, KeyboardMap);

}

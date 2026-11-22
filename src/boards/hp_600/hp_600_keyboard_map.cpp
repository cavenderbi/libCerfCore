#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/virtual_keys.h>
#include "../jornada/jornada_keyboard_layers.h"
#include "../matrix_keyboard_map.h"
#include "hp_600_family_id.h"
#include "hp_600_input_nets.h"
#include "hp_600_keyboard_layout.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

namespace {

constexpr uint8_t kCellAltGr = kVkF10;

constexpr uint8_t kAltLayerId   = 2u;
constexpr uint8_t kAltGrLayerId = 3u;

constexpr jornada_kbd::LayerKey kContrastLayer[] = {
    {0xBCu, "Contrast -"},
    {0xBEu, "Contrast +"},
};

struct ButtonKey {
    Hp600InputNet net;
    uint8_t       vk;
    const char*   label;
};

constexpr ButtonKey kButtonKeys[] = {
    {Hp600InputNet::kPlayButtonUp, 0xB0u, "Play Next"},
    {Hp600InputNet::kRewindButtonUp, 0xB1u, "Play Previous"},
    {Hp600InputNet::kRecordButtonUp, 0x7Bu, "Record"},
};

class Hp600KeyboardMap : public MatrixKeyboardMap {
public:
    using MatrixKeyboardMap::MatrixKeyboardMap;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        const Hp600KeyboardLayout& layout = emu_.Get<Hp600KeyboardLayout>();
        for (uint32_t cell = 0; cell < kHp600KeyCells; ++cell) {
            const uint8_t vk = layout.CellVk()[cell];
            if (vk == 0u || !layout.Scanned(cell)) continue;
            if (vk == kCellAltGr) {
                AddBinding({kVkRMenu, cell, nullptr, 0u, kAltGrLayerId});
                continue;
            }
            AddCell(vk, cell, nullptr, vk == kVkLMenu ? kAltLayerId : uint8_t{0u});
            for (const Hp600HostAlias& a : layout.HostAliases()) {
                if (a.guest_vk == vk) AddBinding({a.host_vk, cell, a.label, 0u, 0u});
            }
        }
        const std::optional<uint32_t> led_cell = layout.LedButtonCell();
        if (led_cell && layout.CellVk()[*led_cell] == 0u) AddBinding({kHp600LedButtonHostVk, *led_cell, nullptr, 0u, 0u});
        const Hp600InputNets& nets = emu_.Get<Hp600InputNets>();
        for (const ButtonKey& k : kButtonKeys) {
            if (!nets.Wired(k.net)) continue;
            AddBinding({k.vk, kHp600InputNetCodeBase + static_cast<uint32_t>(k.net), k.label, 0u, 0u});
        }
        for (const jornada_kbd::LayerKey& k : jornada_kbd::kAltLayer) AddLayerLabel(k.vk, k.label, kAltLayerId);
        for (const jornada_kbd::LayerKey& k : kContrastLayer) AddLayerLabel(k.vk, k.label, kAltLayerId);
        for (const jornada_kbd::LayerKey& k : layout.AltGrLayer()) AddLayerLabel(k.vk, k.label, kAltGrLayerId);
    }
};

}

REGISTER_SERVICE_AS(Hp600KeyboardMap, KeyboardMap);

}

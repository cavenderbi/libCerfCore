#include "../matrix_keyboard_map.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/virtual_keys.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_key_matrix.h"
#include "casio_cassiopeia_a10_members.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kTableColumns = 8u;
constexpr uint32_t kTableEntries = CasioCassiopeiaA10KeyMatrix::kRows * kTableColumns;
constexpr uint32_t kHighRow      = 8u;
constexpr uint32_t kHighRowBit0  = 8u;
constexpr uint8_t  kNoKey          = 0x00u;
constexpr uint8_t  kBacklightKey   = 0xFFu;
constexpr uint8_t  kBacklightHost  = kVkF9;
constexpr uint8_t  kOutsideKeyVk   = 0xFEu;
constexpr uint8_t  kInputPanelHost = kVkF10;

constexpr uint32_t kNoKeyboardInputPanelCell = 1u * CasioCassiopeiaA10KeyMatrix::kColumns + 7u;
constexpr uint32_t kNoKeyboardBacklightCell  = kHighRow * CasioCassiopeiaA10KeyMatrix::kColumns + kHighRowBit0 + 5u;

constexpr char kBacklightLabel[]  = "Backlight";
constexpr char kInputPanelLabel[] = "Input panel";

struct KeyTable {
    uint8_t vk[kTableEntries];
    uint8_t high_row_columns;
    bool    off_is_on_key;
};

constexpr KeyTable kA11Table{
    {
        0xC0u, 0x1Bu, 0x09u, 0x39u, 0x30u, 0xBDu, 0xBBu, 0x08u,
        0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x38u,
        0x51u, 0x57u, 0x45u, 0x52u, 0x54u, 0x59u, 0x55u, 0x49u,
        0x41u, 0x53u, 0x44u, 0x46u, 0x47u, 0x48u, 0x4Au, 0x4Bu,
        0x5Au, 0x58u, 0x43u, 0x56u, 0x42u, 0x4Eu, 0x4Du, 0x4Cu,
        0x00u, 0x0Du, 0xDEu, 0xDCu, 0xDBu, 0xDDu, 0x50u, 0x4Fu,
        0xBCu, 0xBEu, 0xBFu, 0xBAu, 0x26u, 0x28u, 0x25u, 0x27u,
        0x20u, 0xDFu, 0xFFu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0xA2u, 0xA4u, 0xA0u, 0xA1u, 0x5Bu, 0x00u, 0x00u, 0xFEu,
    },
    5u,
    false,
};

constexpr KeyTable kA51Table{
    {
        0x1Bu, 0x09u, 0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0xF0u,
        0x36u, 0x37u, 0x38u, 0x39u, 0x30u, 0xBDu, 0xBBu, 0xFFu,
        0x51u, 0x57u, 0x45u, 0x52u, 0x54u, 0x59u, 0x55u, 0x49u,
        0x41u, 0x53u, 0x44u, 0x46u, 0x47u, 0x48u, 0x4Fu, 0x50u,
        0x5Au, 0x58u, 0x43u, 0x56u, 0x42u, 0x4Eu, 0x4Au, 0x4Bu,
        0x4Du, 0xBCu, 0x4Cu, 0xBEu, 0xBAu, 0xDBu, 0xBFu, 0xDEu,
        0x08u, 0xC0u, 0xDDu, 0xDCu, 0x26u, 0x0Du, 0xE2u, 0x00u,
        0x1Du, 0x20u, 0x1Cu, 0xF2u, 0x25u, 0x28u, 0x27u, 0x19u,
        0xA2u, 0xA4u, 0xA1u, 0xA0u, 0x5Bu, 0x00u, 0xF5u, 0x00u,
    },
    8u,
    true,
};

class CasioCassiopeiaA10KeyboardMap : public MatrixKeyboardMap {
public:
    using MatrixKeyboardMap::MatrixKeyboardMap;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        const std::string_view id = emu_.Get<BoardContext>().GetBoardId();
        AddCell(kOutsideKeyVk, CasioCassiopeiaA10KeyMatrix::kOutsideKey, nullptr, 0u);
        if (!CasioCassiopeiaA10HasKeyboard(id)) {
            AddCell(kInputPanelHost, kNoKeyboardInputPanelCell, kInputPanelLabel, 0u);
            AddCell(kBacklightHost, kNoKeyboardBacklightCell, kBacklightLabel, 0u);
            AddCell(kVkOff, CasioCassiopeiaA10KeyMatrix::kOnKey, nullptr, 0u);
            return;
        }
        const KeyTable& table = CasioCassiopeiaA10IsA51Class(id) ? kA51Table : kA11Table;
        for (uint32_t index = 0; index < kTableEntries; ++index) {
            const uint32_t row    = index / kTableColumns;
            const uint32_t column = index % kTableColumns;
            const uint8_t  vk     = table.vk[index];
            if (vk == kNoKey) continue;
            if (row == kHighRow && column >= table.high_row_columns) continue;
            const uint32_t bit  = row == kHighRow ? kHighRowBit0 + column : column;
            const uint32_t cell = row * CasioCassiopeiaA10KeyMatrix::kColumns + bit;
            if (vk == kBacklightKey) {
                AddCell(kBacklightHost, cell, kBacklightLabel, 0u);
                continue;
            }
            AddCell(vk, cell, nullptr, 0u);
        }
        if (table.off_is_on_key) AddCell(kVkOff, CasioCassiopeiaA10KeyMatrix::kOnKey, nullptr, 0u);
    }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10KeyboardMap, KeyboardMap);

}

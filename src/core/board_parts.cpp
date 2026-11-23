#include "board_parts.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/board_database.h>
#include <cerfcore/core/cerf_emulator.h>
#include "board_config.h"

namespace cerfcore {

REGISTER_SERVICE(BoardParts);

void BoardParts::OnReady() {
    const auto&    db     = emu_.Get<BoardDatabase>();
    const auto&    config = emu_.Get<BoardConfig>();
    const DbBoard& board  = *db.FindBoard(emu_.Get<BoardContext>().GetBoardId());
    parts_ = db.ComposeParts(board, [&config](const DbBoardConfigSetting& s) { return config.SelectedPart(s.param_id); });
}

std::optional<DbLcdPanelSize> BoardParts::ViewportClamp() const {
    for (const DbPart* p : parts_)
        if (p->clamp_viewport) return DbLcdPanelSize{*p->width, *p->height};
    return std::nullopt;
}

const DbPart* BoardParts::Find(std::string_view generic_part_id) const {
    for (const DbPart* p : parts_)
        if (p->generic_part_id == generic_part_id) return p;
    return nullptr;
}

bool BoardParts::Has(std::string_view part_id) const {
    for (const DbPart* p : parts_)
        if (p->id == part_id) return true;
    return false;
}

}

#include <cerfcore/boards/board_context.h>

#include "hp_jornada_540_family_id.h"
#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

namespace {

class HpJornada540Context : public BoardContext {
public:
    using BoardContext::BoardContext;

    std::string_view GetBoardFamilyId() const override { return BoardFamilyId::HpJornada540; }
};

}

REGISTER_SERVICE_AS(HpJornada540Context, BoardContext);

}

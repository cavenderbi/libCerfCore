#include <cerfcore/boards/board_context.h>

#include "nec_mobilepro_200_family_id.h"
#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

namespace {

class NecMobilepro200Context : public BoardContext {
public:
    using BoardContext::BoardContext;

    std::string_view GetBoardFamilyId() const override { return BoardFamilyId::NecMobilepro200; }
};

}

REGISTER_SERVICE_AS(NecMobilepro200Context, BoardContext);

}

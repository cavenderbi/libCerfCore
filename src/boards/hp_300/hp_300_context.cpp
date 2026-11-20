#include <cerfcore/boards/board_context.h>

#include "hp_300_family_id.h"
#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

namespace {

constexpr uint32_t kGuestAdditionsWindowBase = 0x09000000u;

class Hp300Context : public BoardContext {
public:
    using BoardContext::BoardContext;

    std::string_view GetBoardFamilyId() const override { return BoardFamilyId::Hp300; }
    uint32_t         GuestAdditionsWindowBase() const override { return kGuestAdditionsWindowBase; }
};

}

REGISTER_SERVICE_AS(Hp300Context, BoardContext);

}

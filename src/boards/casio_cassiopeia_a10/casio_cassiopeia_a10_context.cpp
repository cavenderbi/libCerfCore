#include <cerfcore/boards/board_context.h>

#include "casio_cassiopeia_a10_family_id.h"
#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

namespace {

constexpr uint32_t kGuestAdditionsWindowBase = 0x04000000u;

class CasioCassiopeiaA10Context : public BoardContext {
public:
    using BoardContext::BoardContext;

    std::string_view GetBoardFamilyId() const override { return BoardFamilyId::CasioCassiopeiaA10; }
    uint32_t         GuestAdditionsWindowBase() const override { return kGuestAdditionsWindowBase; }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10Context, BoardContext);

}

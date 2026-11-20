#include <cerfcore/boards/board_context.h>

#include "casio_cassiopeia_a10_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/casio_dictionary_rom/casio_dictionary_rom.h"
#include "../../socs/sh77xx/sh77xx_area1.h"

namespace cerfcore {

namespace {

constexpr uint32_t kGuestAdditionsWindowBase = kSh77xxArea1PaBase + kCasioDictionaryRomBytes;

class CasioCassiopeiaA10Context : public BoardContext {
public:
    using BoardContext::BoardContext;

    std::string_view GetBoardFamilyId() const override { return BoardFamilyId::CasioCassiopeiaA10; }
    uint32_t         GuestAdditionsWindowBase() const override { return kGuestAdditionsWindowBase; }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10Context, BoardContext);

}

#include "casio_cassiopeia_a10_audio_layout.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10HalfwordAudioLayout : public CasioCassiopeiaA10AudioLayout {
public:
    using CasioCassiopeiaA10AudioLayout::CasioCassiopeiaA10AudioLayout;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               !CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    uint32_t UnitShift() const override { return 0u; }
    uint32_t SlotBytes() const override { return 2u; }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10HalfwordAudioLayout, CasioCassiopeiaA10AudioLayout);

}

#include "casio_cassiopeia_a10_audio_status_clear.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10WriteAudioStatusClear : public CasioCassiopeiaA10AudioStatusClear {
public:
    using CasioCassiopeiaA10AudioStatusClear::CasioCassiopeiaA10AudioStatusClear;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               !CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    bool ClearsOnRead() const override { return false; }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10WriteAudioStatusClear, CasioCassiopeiaA10AudioStatusClear);

}

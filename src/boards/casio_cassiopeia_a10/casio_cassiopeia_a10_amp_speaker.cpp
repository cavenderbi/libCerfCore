#include "casio_cassiopeia_a10_speaker.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_amp_control.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10AmpSpeaker : public CasioCassiopeiaA10Speaker {
public:
    using CasioCassiopeiaA10Speaker::CasioCassiopeiaA10Speaker;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { amps_ = &emu_.Get<CasioCassiopeiaA10AmpControl>(); }

    bool On() const override { return amps_->SpeakerOn(); }

private:
    const CasioCassiopeiaA10AmpControl* amps_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10AmpSpeaker, CasioCassiopeiaA10Speaker);

}

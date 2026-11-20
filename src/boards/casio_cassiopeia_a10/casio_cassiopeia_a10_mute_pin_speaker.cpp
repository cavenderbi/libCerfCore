#include "casio_cassiopeia_a10_speaker.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "casio_cassiopeia_a10_audio_mute_pin.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10MutePinSpeaker : public CasioCassiopeiaA10Speaker {
public:
    using CasioCassiopeiaA10Speaker::CasioCassiopeiaA10Speaker;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               !CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { wiring_ = &emu_.Get<Sh77xxPortWiring>(); }

    bool On() const override {
        return (wiring_->NetLevels(kCasioCassiopeiaA10AudioMutePort) & kCasioCassiopeiaA10AudioMutePin) == 0u;
    }

private:
    const Sh77xxPortWiring* wiring_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10MutePinSpeaker, CasioCassiopeiaA10Speaker);

}

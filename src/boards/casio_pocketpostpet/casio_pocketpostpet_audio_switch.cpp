#include "../../socs/vr41xx/vr41xx_audio_switch.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_pocketpostpet_asic.h"
#include "casio_pocketpostpet_family_id.h"

#include <utility>

namespace cerfcore {

namespace {

class CasioPocketpostpetAudioSwitch : public Vr41xxAudioSwitch {
public:
    using Vr41xxAudioSwitch::Vr41xxAudioSwitch;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioPocketpostpet;
    }

    bool SpeakerOn() override { return !emu_.Get<CasioPocketpostpetAsic>().SpeakerMuted(); }
    bool MicOn() override { return emu_.Get<CasioPocketpostpetAsic>().MicPowered(); }

    void RegisterChangeListener(std::function<void()> fn) override {
        emu_.Get<CasioPocketpostpetAsic>().RegisterAudioControlListener(std::move(fn));
    }
};

}

REGISTER_SERVICE_AS(CasioPocketpostpetAudioSwitch, Vr41xxAudioSwitch);

}

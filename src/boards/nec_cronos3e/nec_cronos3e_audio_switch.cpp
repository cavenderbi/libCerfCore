#include "../../socs/vr41xx/vr41xx_audio_switch.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_cronos3e_family_id.h"

namespace cerfcore {

namespace {

class NecCronos3eAudioSwitch : public Vr41xxGpioAudioSwitch {
public:
    using Vr41xxGpioAudioSwitch::Vr41xxGpioAudioSwitch;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecCronos3e;
    }

    bool MicOn() override { return true; }

protected:
    int            SpeakerGpio() const override    { return 44; }
    Vr41xxPinDrive SpeakerOnLevel() const override { return Vr41xxPinDrive::kHigh; }
};

}

REGISTER_SERVICE_AS(NecCronos3eAudioSwitch, Vr41xxAudioSwitch);

}

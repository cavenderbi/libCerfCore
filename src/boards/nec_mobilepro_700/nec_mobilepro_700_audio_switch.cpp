#include "../../socs/vr41xx/vr41xx_audio_switch.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_mobilepro_700_family_id.h"

namespace cerfcore {

namespace {

class NecMobilepro700AudioSwitch : public Vr41xxGpioAudioSwitch {
public:
    using Vr41xxGpioAudioSwitch::Vr41xxGpioAudioSwitch;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro700;
    }

    bool MicOn() override { return true; }

protected:
    int            SpeakerGpio() const override    { return 12; }
    Vr41xxPinDrive SpeakerOnLevel() const override { return Vr41xxPinDrive::kHigh; }
};

}

REGISTER_SERVICE_AS(NecMobilepro700AudioSwitch, Vr41xxAudioSwitch);

}

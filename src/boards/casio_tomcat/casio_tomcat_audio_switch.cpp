#include "../../socs/vr41xx/vr41xx_audio_switch.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_tomcat_family_id.h"

namespace cerfcore {

namespace {

class CasioTomcatAudioSwitch : public Vr41xxGpioAudioSwitch {
public:
    using Vr41xxGpioAudioSwitch::Vr41xxGpioAudioSwitch;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioTomcat;
    }

    bool MicOn() override { return PinAt(37, Vr41xxPinDrive::kLow); }

protected:
    int            SpeakerGpio() const override    { return 27; }
    Vr41xxPinDrive SpeakerOnLevel() const override { return Vr41xxPinDrive::kLow; }
};

}

REGISTER_SERVICE_AS(CasioTomcatAudioSwitch, Vr41xxAudioSwitch);

}

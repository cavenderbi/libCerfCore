#include "../sh77xx_adc_microphone.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

class HpJornada540Microphone : public Sh77xxAdcMicrophone {
public:
    using Sh77xxAdcMicrophone::Sh77xxAdcMicrophone;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }
};

}

REGISTER_SERVICE_AS(HpJornada540Microphone, Sh77xxAdcMicrophone);

}

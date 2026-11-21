#include "../sh77xx_adc_microphone.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

class Hp600Microphone : public Sh77xxAdcMicrophone {
public:
    using Sh77xxAdcMicrophone::Sh77xxAdcMicrophone;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600; }
};

}

REGISTER_SERVICE_AS(Hp600Microphone, Sh77xxAdcMicrophone);

}

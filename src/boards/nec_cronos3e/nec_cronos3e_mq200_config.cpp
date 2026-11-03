#include "../../peripherals/mediaq_mq200/mediaq_mq200_config.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "nec_cronos3e_family_id.h"

namespace cerfcore {

namespace {

class NecCronos3eMq200Config : public MediaQMq200Config {
public:
    using MediaQMq200Config::MediaQMq200Config;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecCronos3e;
    }

    uint32_t HostWindowBase() const override { return 0x0A000000u; }

    uint32_t OscillatorHz() const override { return 12288000u; }

    uint32_t BusInterfaceMode() const override { return 0x04u; }
};

}

REGISTER_SERVICE_AS(NecCronos3eMq200Config, MediaQMq200Config);

}

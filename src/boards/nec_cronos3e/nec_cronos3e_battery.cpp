#include "../nec_vrc4172_battery.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_cronos3e_family_id.h"

namespace cerfcore {

namespace {

class NecCronos3eBattery : public NecVrc4172Battery {
public:
    using NecVrc4172Battery::NecVrc4172Battery;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecCronos3e;
    }
};

}

REGISTER_SERVICE_AS(NecCronos3eBattery, NecVrc4172Battery);

}

#include "../nec_vrc4172_battery.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_mobilepro_700_family_id.h"

namespace cerfcore {

namespace {

class NecMobilepro700Battery : public NecVrc4172Battery {
public:
    using NecVrc4172Battery::NecVrc4172Battery;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro700;
    }
};

}

REGISTER_SERVICE_AS(NecMobilepro700Battery, NecVrc4172Battery);

}

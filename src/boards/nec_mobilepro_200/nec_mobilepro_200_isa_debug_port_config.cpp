#include "../../peripherals/isa_debug_port/isa_debug_port_config.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/vr4101/vr4101_expansion_bus.h"
#include "nec_mobilepro_200_family_id.h"
#include "nec_mobilepro_200_isa_ports.h"

namespace cerfcore {

namespace {

class NecMobilepro200IsaDebugPortConfig : public IsaDebugPortConfig {
public:
    using IsaDebugPortConfig::IsaDebugPortConfig;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro200;
    }

    uint32_t IsaWindowBase() const override {
        return Vr4101ExpansionIoBase(Vr4101DeviceMode::kEightBit) + kNecMobilepro200IsaDebugBlockPort;
    }
};

}

REGISTER_SERVICE_AS(NecMobilepro200IsaDebugPortConfig, IsaDebugPortConfig);

}

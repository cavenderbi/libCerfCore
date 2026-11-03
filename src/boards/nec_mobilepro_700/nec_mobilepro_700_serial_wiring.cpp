#include "../../socs/vr41xx/vr41xx_serial_wiring.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "nec_mobilepro_700_family_id.h"

namespace cerfcore {

namespace {

class NecMobilepro700SerialWiring : public Vr41xxGpioSuppliedSerialWiring {
public:
    using Vr41xxGpioSuppliedSerialWiring::Vr41xxGpioSuppliedSerialWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro700;
    }

    std::optional<Vr41xxSerialModem> ForSiu() const override {
        return Vr41xxSerialModem("COM1", 5);
    }

protected:
    int SupplyGpio() const override { return 14; }
};

}

REGISTER_SERVICE_AS(NecMobilepro700SerialWiring, Vr41xxSerialWiring);

}

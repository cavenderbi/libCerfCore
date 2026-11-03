#include "../../socs/vr41xx/vr41xx_giu.h"
#include "../../socs/vr41xx/vr41xx_serial_wiring.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "nec_cronos3e_family_id.h"

namespace cerfcore {

namespace {

class NecCronos3eSerialWiring : public Vr41xxGpioSuppliedSerialWiring {
public:
    using Vr41xxGpioSuppliedSerialWiring::Vr41xxGpioSuppliedSerialWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecCronos3e;
    }

    std::optional<Vr41xxSerialModem> ForSiu() const override {
        return Vr41xxSerialModem("COM1", Vr41xxGiu::kDcdPin);
    }

protected:
    int SupplyGpio() const override { return 46; }
};

}

REGISTER_SERVICE_AS(NecCronos3eSerialWiring, Vr41xxSerialWiring);

}

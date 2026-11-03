#include "../../socs/vr41xx/vr41xx_giu.h"
#include "../../socs/vr41xx/vr41xx_serial_wiring.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "casio_tomcat_family_id.h"

namespace cerfcore {

namespace {

class CasioTomcatSerialWiring : public Vr41xxGpioSuppliedSerialWiring {
public:
    using Vr41xxGpioSuppliedSerialWiring::Vr41xxGpioSuppliedSerialWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioTomcat;
    }

    std::optional<Vr41xxSerialModem> ForSiu() const override {
        return Vr41xxSerialModem("COM1", Vr41xxGiu::kDcdPin);
    }

protected:
    int SupplyGpio() const override { return 38; }
};

}

REGISTER_SERVICE_AS(CasioTomcatSerialWiring, Vr41xxSerialWiring);

}

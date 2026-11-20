#include "../../socs/vr41xx/vr41xx_giu.h"
#include "../../socs/vr41xx/vr41xx_serial_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_mobilepro_200_family_id.h"

namespace cerfcore {

namespace {

constexpr char kSerialPortId[]    = "serial_port";
constexpr char kSerialPortLabel[] = "COM1";

class NecMobilepro200SerialWiring : public Vr41xxGpioSuppliedSerialWiring {
public:
    using Vr41xxGpioSuppliedSerialWiring::Vr41xxGpioSuppliedSerialWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro200;
    }

    std::optional<Vr41xxSerialModem> ForSiu() const override {
        return Vr41xxSerialModem({kSerialPortId, kSerialPortLabel}, Vr41xxGiu::kDcdPin);
    }

protected:
    int SupplyGpio() const override { return 11; }
};

}

REGISTER_SERVICE_AS(NecMobilepro200SerialWiring, Vr41xxSerialWiring);

}

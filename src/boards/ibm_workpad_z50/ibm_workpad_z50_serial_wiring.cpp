#include "../../socs/vr41xx/vr41xx_giu.h"
#include "../../socs/vr41xx/vr41xx_serial_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "ibm_workpad_z50_family_id.h"
#include "ibm_workpad_z50_gpio_buffers.h"

#include <utility>

namespace cerfcore {

namespace {

class IbmWorkpadZ50SerialWiring : public Vr41xxSerialWiring {
public:
    using Vr41xxSerialWiring::Vr41xxSerialWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
    }

    std::optional<Vr41xxSerialModem> ForSiu() const override {
        return Vr41xxSerialModem("COM1", Vr41xxGiu::kDcdPin);
    }

    bool SupplyOn() override { return emu_.Get<IbmWorkpadZ50GpioBuffers>().SerialDriversPowered(); }

    void RegisterSupplyListener(std::function<void()> fn) override {
        emu_.Get<IbmWorkpadZ50GpioBuffers>().RegisterOutputListener(std::move(fn));
    }
};

}

REGISTER_SERVICE_AS(IbmWorkpadZ50SerialWiring, Vr41xxSerialWiring);

}

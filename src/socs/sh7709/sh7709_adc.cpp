#include "../sh77xx/sh77xx_adc.h"
#include "../sh77xx/sh77xx_pfc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7709_id.h"

namespace cerfcore {

namespace {

class Sh7709Adc : public Sh77xxAdc {
public:
    using Sh77xxAdc::Sh77xxAdc;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7709; }

protected:
    ControlModel Control() const override { return ControlModel{0xC0u, 0x3Fu, 0x00u, 0x00u, false}; }
    uint8_t      Stbcr2ModuleStop() const override { return kStbcr2Mstp5; }
    TriggerPin   AdTriggerPin() const override { return TriggerPin{kSh77xxPortH, 5u}; }

private:
    static constexpr uint8_t kStbcr2Mstp5 = 0x04u;
};

}

REGISTER_SERVICE_AS(Sh7709Adc, Sh77xxAdc);

}

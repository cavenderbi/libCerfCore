#include "../sh77xx/sh77xx_adc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7729_id.h"

namespace cerfcore {

namespace {

class Sh7729Adc : public Sh77xxAdc {
public:
    using Sh77xxAdc::Sh77xxAdc;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7729; }

protected:
    ControlModel Control() const override { return ControlModel{0xE0u, 0x07u, 0x18u, 0x20u, false}; }
    uint8_t      Stbcr2ModuleStop() const override { return kStbcr2Mstp5; }

private:
    static constexpr uint8_t kStbcr2Mstp5 = 0x04u;
};

}

REGISTER_SERVICE_AS(Sh7729Adc, Sh77xxAdc);

}

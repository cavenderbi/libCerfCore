#include "../sh77xx/sh77xx_dmac.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7729_id.h"

namespace cerfcore {

namespace {

class Sh7729Dmac : public Sh77xxDmac {
public:
    using Sh77xxDmac::Sh77xxDmac;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7729; }

protected:
    StandbyEffect Standby() const override { return StandbyEffect::kClearsDmaor; }
    uint32_t      ExternalRequestControls() const override { return kChcrRlAmAlDs; }
    uint8_t       Stbcr2ModuleStop() const override { return kStbcr2Mstp7; }

private:
    static constexpr uint32_t kChcrRlAmAlDs = 0x00070040u;
    static constexpr uint8_t  kStbcr2Mstp7  = 0x10u;
};

}

REGISTER_SERVICE_AS(Sh7729Dmac, Sh77xxDmac);

}

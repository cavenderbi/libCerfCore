#include "../sh77xx/sh77xx_dmac.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7707_id.h"

namespace cerfcore {

namespace {

class Sh7707Dmac : public Sh77xxDmac {
public:
    using Sh77xxDmac::Sh77xxDmac;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }

protected:
    StandbyEffect Standby() const override { return StandbyEffect::kHolds; }
    uint32_t      ExternalRequestControls() const override { return kChcrAmAlDs; }
    uint8_t       Stbcr2ModuleStop() const override { return kStbcr2Mstp10; }

private:
    static constexpr uint32_t kChcrAmAlDs   = 0x00030040u;
    static constexpr uint8_t  kStbcr2Mstp10 = 0x80u;
};

}

REGISTER_SERVICE_AS(Sh7707Dmac, Sh77xxDmac);

}

#include "../sh77xx/sh77xx_dma_address_map.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7729_id.h"

namespace cerfcore {

namespace {

class Sh7729DmaAddressMap : public Sh77xxDmaAddressMap {
public:
    using Sh77xxDmaAddressMap::Sh77xxDmaAddressMap;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7729; }

protected:
    bool ReachesChipModule(uint32_t, uint32_t) const override { return false; }
};

}

REGISTER_SERVICE_AS(Sh7729DmaAddressMap, Sh77xxDmaAddressMap);

}

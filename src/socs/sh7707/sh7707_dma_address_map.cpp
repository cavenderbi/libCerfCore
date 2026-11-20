#include "../sh77xx/sh77xx_dma_address_map.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kPccFirst = 0x040000E0u;
constexpr uint32_t kPccEnd   = 0x040000F8u;

class Sh7707DmaAddressMap : public Sh77xxDmaAddressMap {
public:
    using Sh77xxDmaAddressMap::Sh77xxDmaAddressMap;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }

protected:
    bool ReachesChipModule(uint32_t pa, uint32_t bytes) const override {
        return pa >= kPccFirst && pa + bytes <= kPccEnd;
    }
};

}

REGISTER_SERVICE_AS(Sh7707DmaAddressMap, Sh77xxDmaAddressMap);

}

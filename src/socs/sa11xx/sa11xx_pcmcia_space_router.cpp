#include "../../peripherals/pcmcia/pcmcia_space_router.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/pcmcia/pcmcia_slot.h"
#include "sa1100_id.h"
#include "sa1110_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

class Sa11xxPcmciaSpaceRouter : public PcmciaSpaceRouter {
public:
    using PcmciaSpaceRouter::PcmciaSpaceRouter;

    bool ShouldRegister() override {
        const std::string_view soc = emu_.Get<BoardContext>().GetSocId();
        return soc == SocId::Sa1100 || soc == SocId::Sa1110;
    }

    uint32_t ReadWord(uint32_t addr) override {
        RequireIois16(addr, 0u);
        const uint32_t low  = ReadHalf(addr);
        const uint32_t high = floating_.Read16(kFloatingTag, addr + 2u);
        return low | (high << 16);
    }

    void WriteWord(uint32_t addr, uint32_t value) override {
        RequireIois16(addr, value);
        WriteHalf(addr, static_cast<uint16_t>(value));
        floating_.Write(kFloatingTag, 16u, addr + 2u);
    }

private:
    void RequireIois16(uint32_t addr, uint32_t value) {
        Region region;
        uint32_t off;
        PcmciaSlot* slot = Decode(addr, &region, &off);
        if (slot && region == Region::Io && !slot->Iois16(off)) {
            HaltUnsupportedAccess("SA-11xx PCMCIA 32-bit I/O access without nIOIS16", addr, value);
        }
    }
};

}

REGISTER_SERVICE_AS(Sa11xxPcmciaSpaceRouter, PcmciaSpaceRouter);

}

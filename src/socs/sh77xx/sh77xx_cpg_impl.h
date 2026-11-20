#pragma once

#include <cstdint>
#include <iterator>
#include <string_view>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../cpu/sh/sh_board_clock.h"
#include "sh77xx_cpg.h"

namespace cerfcore {

struct Sh77xxFrqcrSetting {
    uint16_t frqcr;
    uint8_t  cpu_per_ckio;
    uint8_t  ckio_per_pclk;
};

struct Sh77xxFrqcrModeTable {
    const Sh77xxFrqcrSetting* settings;
    uint32_t                  count;
    uint16_t                  rate_bits;
};

struct Sh77xxCpgModel {
    Sh77xxFrqcrModeTable pll1_switchable;
    Sh77xxFrqcrModeTable pll1_always_on;
};

inline constexpr uint16_t kSh77xxFrqcrReset = 0x0102u;

template <const std::string_view& Soc, const Sh77xxCpgModel& M>
class Sh77xxCpgBase : public Sh77xxCpg {
public:
    using Sh77xxCpg::Sh77xxCpg;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == Soc;
    }

    GuestCycleClock::Rate ResetCpuRate() const override { return CpuRate(kSh77xxFrqcrReset); }

    GuestCycleClock::Rate CpuRate(uint16_t frqcr) const override {
        return GuestCycleClock::Rate{BoardCkioHz() * Setting(frqcr).cpu_per_ckio, 1u};
    }

    GuestCycleClock::Rate PeripheralRate(uint16_t frqcr) const override {
        return GuestCycleClock::Rate{BoardCkioHz(), Setting(frqcr).ckio_per_pclk};
    }

private:
    const Sh77xxFrqcrSetting& Setting(uint16_t frqcr) const {
        const bool                  switchable = emu_.Get<ShBoardClock>().Pll1Switchable();
        const Sh77xxFrqcrModeTable& modes      = switchable ? M.pll1_switchable : M.pll1_always_on;
        for (uint32_t i = 0; i < modes.count; ++i) {
            if (((modes.settings[i].frqcr ^ frqcr) & modes.rate_bits) == 0u) return modes.settings[i];
        }
        emu_.Get<Fatal>().Die("Sh77xxCpg: FRQCR 0x%04X has no setting row, PLL circuit 1 %s", frqcr,
                              switchable ? "switchable" : "always on");
    }
};

}

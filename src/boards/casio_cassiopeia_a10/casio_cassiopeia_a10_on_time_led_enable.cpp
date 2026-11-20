#include "casio_cassiopeia_a10_led_enable.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint16_t kOnTimeEnable = 0x0080u;

class CasioCassiopeiaA10OnTimeLedEnable : public CasioCassiopeiaA10LedEnable {
public:
    using CasioCassiopeiaA10LedEnable::CasioCassiopeiaA10LedEnable;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               !CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    CasioCassiopeiaA10LedMode Mode(uint16_t on_time) const override {
        return (on_time & kOnTimeEnable) != 0u ? CasioCassiopeiaA10LedMode::kBlinking : CasioCassiopeiaA10LedMode::kOff;
    }

    void AddChangeListener(CallbackList<>::Callback) override {}
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10OnTimeLedEnable, CasioCassiopeiaA10LedEnable);

}

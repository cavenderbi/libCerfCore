#include "casio_cassiopeia_a10_led_enable.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_led_control.h"
#include "casio_cassiopeia_a10_members.h"

#include <cstdint>
#include <utility>

namespace cerfcore {

namespace {

class CasioCassiopeiaA10ControlLedEnable : public CasioCassiopeiaA10LedEnable {
public:
    using CasioCassiopeiaA10LedEnable::CasioCassiopeiaA10LedEnable;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { control_ = &emu_.Get<CasioCassiopeiaA10LedControl>(); }

    CasioCassiopeiaA10LedMode Mode(uint16_t) const override {
        if (control_->Steady()) return CasioCassiopeiaA10LedMode::kSteady;
        return control_->Blinking() ? CasioCassiopeiaA10LedMode::kBlinking : CasioCassiopeiaA10LedMode::kOff;
    }

    void AddChangeListener(CallbackList<>::Callback listener) override {
        control_->AddChangeListener(std::move(listener));
    }

private:
    CasioCassiopeiaA10LedControl* control_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10ControlLedEnable, CasioCassiopeiaA10LedEnable);

}

#include "../../peripherals/epson_s1d13705/s1d13705_config.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_jornada_520_id.h"
#include "hp_jornada_525_id.h"
#include "hp_jornada_540_family_id.h"
#include "hp_jornada_540_lcd_clock.h"

namespace cerfcore {

namespace {

constexpr uint32_t kS1d13705Pa = 0x14000000u;

class HpJornada540S1d13705Config : public S1d13705Config {
public:
    using S1d13705Config::S1d13705Config;

    bool ShouldRegister() override {
        auto& board = emu_.Get<BoardContext>();
        if (board.GetBoardFamilyId() != BoardFamilyId::HpJornada540) return false;
        return board.GetBoardId() == BoardId::HpJornada520 || board.GetBoardId() == BoardId::HpJornada525;
    }

    void OnReady() override {
        clock_ = &emu_.Get<HpJornada540LcdClock>();
    }

    uint32_t              HostWindowBase() const override { return kS1d13705Pa; }
    GuestCycleClock::Rate ClkiRate() const override { return clock_->Rate(); }
    void RegisterClkiListener(std::function<void()> fn) override { clock_->RegisterListener(std::move(fn)); }

private:
    HpJornada540LcdClock* clock_ = nullptr;
};

}

REGISTER_SERVICE_AS(HpJornada540S1d13705Config, S1d13705Config);

}

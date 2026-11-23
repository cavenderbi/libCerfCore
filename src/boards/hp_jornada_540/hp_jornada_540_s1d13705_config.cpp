#include "../../peripherals/epson_s1d13705/s1d13705_config.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../core/board_parts.h"
#include "../../peripherals/epson_s1d13705/epson_s1d13705_part_id.h"
#include "hp_jornada_540_display_controller_pa.h"
#include "hp_jornada_540_family_id.h"
#include "hp_jornada_540_lcd_clock.h"

namespace cerfcore {

namespace {

class HpJornada540S1d13705Config : public S1d13705Config {
public:
    using S1d13705Config::S1d13705Config;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540 &&
               emu_.Get<BoardParts>().Has(PartId::EpsonS1d13705);
    }

    void OnReady() override {
        clock_ = &emu_.Get<HpJornada540LcdClock>();
    }

    uint32_t              HostWindowBase() const override { return kHpJornada540DisplayControllerPa; }
    GuestCycleClock::Rate ClkiRate() const override { return clock_->Rate(); }
    void RegisterClkiListener(std::function<void()> fn) override { clock_->RegisterListener(std::move(fn)); }

private:
    HpJornada540LcdClock* clock_ = nullptr;
};

}

REGISTER_SERVICE_AS(HpJornada540S1d13705Config, S1d13705Config);

}

#include "../../peripherals/epson_s1d13505/s1d13505_config.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_jornada_540_display_controller_pa.h"
#include "hp_jornada_540_family_id.h"
#include "hp_jornada_540_lcd_clock.h"
#include "hp_jornada_545_id.h"
#include "hp_jornada_547_id.h"
#include "hp_jornada_548_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kLittleEndianPin = 0x0010u;

class HpJornada540S1d13505Config : public S1d13505Config {
public:
    using S1d13505Config::S1d13505Config;

    bool ShouldRegister() override {
        auto& board = emu_.Get<BoardContext>();
        if (board.GetBoardFamilyId() != BoardFamilyId::HpJornada540) return false;
        const auto id = board.GetBoardId();
        return id == BoardId::HpJornada545 || id == BoardId::HpJornada547 || id == BoardId::HpJornada548;
    }

    void OnReady() override {
        clock_ = &emu_.Get<HpJornada540LcdClock>();
    }

    uint32_t              HostWindowBase() const override { return kHpJornada540DisplayControllerPa; }
    GuestCycleClock::Rate ClkiRate() const override { return clock_->Rate(); }
    void RegisterClkiListener(std::function<void()> fn) override { clock_->RegisterListener(std::move(fn)); }
    uint16_t ConfigurationPins() const override { return kLittleEndianPin; }
    uint8_t  GpioInputLevels() const override { return 0u; }

private:
    HpJornada540LcdClock* clock_ = nullptr;
};

}

REGISTER_SERVICE_AS(HpJornada540S1d13505Config, S1d13505Config);

}

#include "hp_300_battery.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/board_database.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include "../../core/board_config.h"
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "../main_batteries_param_id.h"
#include "hp_300_family_id.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace cerfcore {

REGISTER_SERVICE(Hp300Battery);

namespace {

constexpr uint16_t kFullUnderCharge = 0x3C2u;
constexpr uint16_t kAlkalineLevelAdc[] = {0x297u, 0x2B5u, 0x2CCu, 0x2E2u, 0x2F6u, 0x306u, 0x318u, 0x331u, 0x3C3u};
constexpr uint16_t kNicdNimhLevelAdc[] = {0x297u, 0x2E8u, 0x2FCu, 0x335u};

constexpr std::pair<std::string_view, std::span<const uint16_t>> kLevelAdc[] = {
    {"alkaline", kAlkalineLevelAdc},
    {"nicd_nimh", kNicdNimhLevelAdc},
};

}

bool Hp300Battery::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
}

void Hp300Battery::OnReady() {
    intc_      = &emu_.Get<Sh77xxIntc>();
    level_adc_ = ValueByName(kLevelAdc, emu_.Get<BoardConfig>().Part(BoardConfigParamId::MainBatteries).chemistry);
    battery_.AddChangeListener([this] { DriveExternalPowerInterruptPin(); });
    emu_.Get<MachineObjectTree>().Add(&battery_);
    DriveExternalPowerInterruptPin();
}

bool Hp300Battery::OnExternalPower() const { return battery_.OnExternalPower(); }

uint16_t Hp300Battery::MainBatteryAdc() const {
    const int      percent = battery_.ChargePercent();
    const int      top     = static_cast<int>(level_adc_.size()) - 1;
    const uint16_t rest =
        level_adc_[static_cast<size_t>((percent * top + Battery::kFullPercent / 2) / Battery::kFullPercent)];
    if (charger_enabled_ && OnExternalPower() && percent == Battery::kFullPercent) return std::max(rest, kFullUnderCharge);
    return rest;
}

void Hp300Battery::SetChargerEnabled(bool enabled) { charger_enabled_ = enabled; }

void Hp300Battery::DriveExternalPowerInterruptPin() {
    intc_->SetPinLevel(Sh77xxIntcPin::kIrq1, battery_.OnBattery());
}

}

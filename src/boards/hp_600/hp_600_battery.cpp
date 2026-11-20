#include "hp_600_battery.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include "hp_600_family_id.h"
#include "hp_600_input_nets.h"
#include "hp_600_main_battery_scale.h"

#include <algorithm>

namespace cerfcore {

REGISTER_SERVICE(Hp600Battery);

namespace {

constexpr int kTopLevel        = 6;
constexpr int kPercentPerLevel = 16;

}

bool Hp600Battery::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
}

void Hp600Battery::OnReady() {
    nets_    = &emu_.Get<Hp600InputNets>();
    scale_   = &emu_.Get<Hp600MainBatteryScale>();
    battery_.AddChangeListener([this] { DriveAcLine(); });
    emu_.Get<MachineObjectTree>().Add(&battery_);
    DriveAcLine();
}

bool Hp600Battery::Charging() const { return battery_.Charging(); }

bool Hp600Battery::OnExternalPower() const { return battery_.OnExternalPower(); }

uint16_t Hp600Battery::MainBatteryAdc() const {
    const int level = std::min((battery_.ChargePercent() + kPercentPerLevel / 2) / kPercentPerLevel, kTopLevel);
    if (level == kTopLevel) return scale_->LevelThreshold(kTopLevel);
    return static_cast<uint16_t>(scale_->LevelThreshold(level) - 1u);
}

void Hp600Battery::DriveAcLine() { nets_->Drive(Hp600InputNet::kAcAdapterOff, battery_.OnBattery()); }

}

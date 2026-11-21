#include "hp_jornada_540_battery.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include "hp_jornada_540_family_id.h"

#include <algorithm>
#include <array>

namespace cerfcore {

namespace {

constexpr int                     kPercentPerLevel   = 25;
constexpr std::array<uint16_t, 5> kLevelCodes        = {0x311u, 0x33Cu, 0x374u, 0x3A8u, 0x3D4u};
constexpr uint16_t                kChargingCorrection = 0x23u;

}

REGISTER_SERVICE(HpJornada540Battery);

bool HpJornada540Battery::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

void HpJornada540Battery::OnReady() { emu_.Get<MachineObjectTree>().Add(&battery_); }

uint16_t HpJornada540Battery::MainBatteryCode() const {
    const int    level = std::min((battery_.ChargePercent() + kPercentPerLevel / 2) / kPercentPerLevel,
                                  static_cast<int>(kLevelCodes.size()) - 1);
    const uint16_t code = kLevelCodes[static_cast<size_t>(level)];
    return battery_.Charging() ? static_cast<uint16_t>(code + kChargingCorrection) : code;
}

}

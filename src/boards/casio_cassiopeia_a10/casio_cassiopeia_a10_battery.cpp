#include "casio_cassiopeia_a10_battery.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include "casio_cassiopeia_a10_family_id.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10Battery);

namespace {

constexpr int kLowBelowPercent      = 33;
constexpr int kCriticalBelowPercent = 5;
constexpr int kEmptyBelowPercent    = 1;

constexpr int kTripBelowPercent[] = {kLowBelowPercent, kCriticalBelowPercent, kEmptyBelowPercent};

}

bool CasioCassiopeiaA10Battery::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
}

void CasioCassiopeiaA10Battery::OnReady() { emu_.Get<MachineObjectTree>().Add(&battery_); }

bool CasioCassiopeiaA10Battery::OnExternalPower() const { return battery_.OnExternalPower(); }

bool CasioCassiopeiaA10Battery::Recharging() const { return battery_.Charging(); }

void CasioCassiopeiaA10Battery::AddChangeListener(std::function<void()> listener) {
    battery_.AddChangeListener(std::move(listener));
}

bool CasioCassiopeiaA10Battery::Tripped(CasioCassiopeiaA10Vdet detector) const {
    return battery_.ChargePercent() < kTripBelowPercent[static_cast<uint32_t>(detector)];
}

}

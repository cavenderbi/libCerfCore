#include "compaq_ipaq_h3000_battery.h"

#include "compaq_ipaq_h3000_battery_curve.h"
#include "compaq_ipaq_h3000_family_id.h"
#include "compaq_ipaq_h3000_ir_power.h"
#include "compaq_ipaq_h3000_micro_display.h"
#include "../../peripherals/pcmcia/pcmcia_slot.h"
#include "../../peripherals/pcmcia/pcmcia_space_router.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>

namespace cerfcore {

namespace {

constexpr uint8_t kChemistryLithiumPolymer = 0x05;
constexpr uint8_t kStatusCharging          = 0x08;
constexpr uint8_t kStatusFull              = 0x40;

constexpr uint32_t kPcCardLoad      = 30;
constexpr uint32_t kIrModuleLoad    = 15;
constexpr uint32_t kHeavyLoad       = 50;
constexpr uint32_t kSagPerLoadLight = 6;
constexpr uint32_t kSagPerLoadHeavy = 11;
constexpr uint32_t kSagShift        = 10;
constexpr uint32_t kSagDivisor      = 30000;

}

REGISTER_SERVICE(CompaqIpaqH3000Battery);

bool CompaqIpaqH3000Battery::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetBoardFamilyId() == BoardFamilyId::CompaqIpaqH3000;
}

void CompaqIpaqH3000Battery::OnReady() { emu_.Get<MachineObjectTree>().Add(&battery_); }

void CompaqIpaqH3000Battery::Reply(uint8_t* out) const {
    const bool     on_ac        = OnAc();
    const int      fill         = battery_.ChargePercent();
    const uint16_t open_circuit = emu_.Get<CompaqIpaqH3000BatteryCurve>().VoltageFor(fill);
    out[0] = on_ac ? 1u : 0u;
    out[1] = kChemistryLithiumPolymer;
    cerf::le::Put16(out + 2, on_ac ? open_circuit : static_cast<uint16_t>(open_circuit - LoadSag()));
    out[4] = !on_ac ? 0u : (Battery::ChargingAt(on_ac, fill) ? kStatusCharging : kStatusFull);
}

bool CompaqIpaqH3000Battery::OnAc() const { return battery_.OnExternalPower(); }

bool CompaqIpaqH3000Battery::Charging() const { return battery_.Charging(); }

bool CompaqIpaqH3000Battery::PcCardPowered() const {
    const auto& router = emu_.Get<PcmciaSpaceRouter>();
    for (int socket = 0; socket < 2; ++socket) {
        const PcmciaSlot* slot = router.Socket(socket);
        if (slot->IsPowered() && slot->HasCard()) return true;
    }
    return false;
}

uint16_t CompaqIpaqH3000Battery::LoadSag() const {
    const uint32_t load = emu_.Get<CompaqIpaqH3000MicroDisplay>().BacklightLoad() +
                          (PcCardPowered() ? kPcCardLoad : 0u) +
                          (emu_.Get<CompaqIpaqH3000IrPower>().Powered() ? kIrModuleLoad : 0u);
    const uint32_t per_load = load > kHeavyLoad ? kSagPerLoadHeavy : kSagPerLoadLight;
    return static_cast<uint16_t>(((per_load * load) << kSagShift) / kSagDivisor);
}

}

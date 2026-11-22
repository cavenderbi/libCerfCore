#include "hp_jornada_540_eeprom_bus.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "hp_jornada_540_eeprom.h"
#include "hp_jornada_540_family_id.h"

#include <vector>

namespace cerfcore {

REGISTER_SERVICE(HpJornada540EepromBus);

bool HpJornada540EepromBus::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

void HpJornada540EepromBus::OnReady() {
    eeprom_ = &emu_.Get<HpJornada540Eeprom>();
    target_.emplace(emu_.Get<GuestCpuReset>(), [this](bool released) { sda_released_ = released; },
                    std::vector<I2cSlave*>{eeprom_},
                    [this](uint8_t addr, bool read) {
                        emu_.Get<Fatal>().Die("HpJornada540EepromBus: no device answers address 0x%02X (%s)", addr,
                                              read ? "read" : "write");
                    });
}

void HpJornada540EepromBus::OnLines(bool scl, bool sda) { target_->OnLines(scl, sda); }

void HpJornada540EepromBus::SaveState(StateWriter& w) {
    target_->SaveState(w);
    w.Write<uint8_t>("eeprom_sda_released", sda_released_ ? 1u : 0u);
    eeprom_->SaveState(w);
}

void HpJornada540EepromBus::RestoreState(StateReader& r) {
    uint8_t released = 0u;
    target_->RestoreState(r);
    r.Read("eeprom_sda_released", released);
    sda_released_ = released != 0u;
    eeprom_->RestoreState(r);
}

}

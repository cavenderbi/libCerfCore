#include "nec_vr41xx_touch_panel.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../socs/vr41xx/vr41xx_piu_regs.h"

namespace cerfcore {

using namespace cerf_vr41xx_piu_detail;

namespace {

constexpr uint16_t kAdin2Reference  = 379u;
constexpr uint16_t kPressureContact = 0x03FFu;

}

std::optional<uint16_t> NecVr41xxTouchPanel::ConvertCommandPort(uint16_t adcmd, uint16_t, uint16_t) {
    switch (adcmd) {
        case kAdcmdAdin0: return MainBatteryWord();
        case kAdcmdAdin1: return BackupCellWord();
        case kAdcmdAdin2: return kAdin2Reference;
        default:
            emu_.Get<Fatal>().Die("NecVr41xxTouchPanel: PIUCMDREG ADCMD=0x%X selects an A/D port this "
                                  "board's panel does not drive", adcmd);
    }
}

std::optional<uint16_t> NecVr41xxTouchPanel::PressureSample() {
    return kPressureContact;
}

std::optional<uint16_t> NecVr41xxTouchPanel::AdPortScanSample(uint16_t port) {
    emu_.Get<Fatal>().Die("NecVr41xxTouchPanel: ADPortScan A/D port 0x%X this board does not drive", port);
}

}

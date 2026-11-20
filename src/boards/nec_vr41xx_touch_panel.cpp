#include "nec_vr41xx_touch_panel.h"

#include "../socs/vr41xx/vr41xx_piu_regs.h"

namespace cerfcore {

using namespace cerf_vr41xx_piu_detail;

namespace {

constexpr uint16_t kAdin2Reference  = 379u;
constexpr uint16_t kPressureContact = 0x03FFu;
constexpr uint16_t kNoPressure      = 0u;

}

std::optional<uint16_t> NecVr41xxTouchPanel::PortSample(uint16_t port) {
    switch (port) {
        case kAdcmdAdin0: return MainBatteryWord();
        case kAdcmdAdin1: return BackupCellWord();
        case kAdcmdAdin2: return kAdin2Reference;
        default:          return kUndrivenPortSample;
    }
}

std::optional<uint16_t> NecVr41xxTouchPanel::PressureSample(bool contact) {
    return contact ? kPressureContact : kNoPressure;
}

uint16_t NecVr41xxTouchPanel::ReleasedPlateSample() { return kUndrivenPortSample; }

}

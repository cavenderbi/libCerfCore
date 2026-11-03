#include "ibm_workpad_z50_battery_smbus.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>
#include "../../socs/vr41xx/vr41xx_giu.h"
#include "ibm_workpad_z50_battery_gauge.h"
#include "ibm_workpad_z50_family_id.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr int kSclGpio = 13;
constexpr int kSdaGpio = 14;

}

REGISTER_SERVICE(IbmWorkpadZ50BatterySmbus);

bool IbmWorkpadZ50BatterySmbus::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
}

void IbmWorkpadZ50BatterySmbus::OnReady() {
    giu_   = &emu_.Get<Vr41xxGiu>();
    gauge_ = &emu_.Get<IbmWorkpadZ50BatteryGauge>();
    target_.emplace([this](bool released) { DriveSda(released); },
                    std::vector<I2cSlave*>{ gauge_ },
                    [this](uint8_t addr, bool read) {
                        if (addr == IbmWorkpadZ50BatteryGauge::kAddress) return;
                        emu_.Get<Fatal>().Die("z50 battery SMBus: no device answers address "
                                              "0x%02X (%s)", addr, read ? "read" : "write");
                    });
    giu_->SetPinLevel(kSclGpio, true);
    giu_->SetPinLevel(kSdaGpio, true);
    giu_->RegisterOutputDriveListener([this] { OnPins(); });
    giu_->RegisterPinDeviceState([this](StateWriter& w) { SaveState(w); },
                                 [this](StateReader& r) { RestoreState(r); });
}

void IbmWorkpadZ50BatterySmbus::OnPins() {
    const bool scl = giu_->OutputDrive(kSclGpio) != Vr41xxPinDrive::kLow;
    const bool sda = giu_->OutputDrive(kSdaGpio) != Vr41xxPinDrive::kLow && sda_released_;
    target_->OnLines(scl, sda);
}

void IbmWorkpadZ50BatterySmbus::DriveSda(bool released) {
    sda_released_ = released;
    giu_->SetPinLevel(kSdaGpio, released);
}

void IbmWorkpadZ50BatterySmbus::SaveState(StateWriter& w) {
    target_->SaveState(w);
    w.Write("smbus_sda_released", static_cast<uint8_t>(sda_released_));
    gauge_->SaveState(w);
}

void IbmWorkpadZ50BatterySmbus::RestoreState(StateReader& r) {
    uint8_t v = 0;
    target_->RestoreState(r);
    r.Read("smbus_sda_released", v);
    sda_released_ = v != 0u;
    gauge_->RestoreState(r);
}

}

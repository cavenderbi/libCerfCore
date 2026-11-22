#include "hp_sh3_serial_port_wiring.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../socs/sh77xx/sh77xx_port_data.h"
#include "../socs/sh77xx/sh77xx_port_wiring.h"
#include "../socs/sh77xx/sh77xx_scif.h"

namespace cerfcore {

void HpSh3SerialPortWiring::OnReady() {
    ports_ = &emu_.Get<Sh77xxPortWiring>();
    emu_.Get<Sh77xxPortData>().AddDriveListener([this] { TrackNets(); });
    TrackNets();
}

bool HpSh3SerialPortWiring::Asserted(const ControlPin& pin) const {
    const bool high = (ports_->NetLevels(pin.port) & pin.mask) != 0u;
    return high == (pin.level == ActiveLevel::kHigh);
}

void HpSh3SerialPortWiring::TrackNets() {
    const ControlPins pins    = Pins();
    const bool        powered = Asserted(pins.driver_power);
    SetLines(kSh77xxScif2Base, powered, powered && Asserted(pins.dtr), powered && Asserted(pins.rts));
}

}

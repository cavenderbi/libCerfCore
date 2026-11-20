#include <cerfcore/host/machine_indicators.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/host_input_activity.h>

namespace cerfcore {

void MachineButton::Press() {
    if (Pressed()) emu_.Get<HostInputActivity>().Delivered();
}

void MachineLamp::Refresh() {
    if (published_.Replace([this] { return Lamp(); })) Changed();
}

void MachineVibrator::Refresh() {
    if (published_.Replace([this] { return Vibrating(); })) Changed();
}

void MachineCharacterDisplay::Refresh() {
    if (published_.Replace([this] { return Characters(); })) Changed();
}

}

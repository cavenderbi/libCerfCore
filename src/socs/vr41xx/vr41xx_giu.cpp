#include "vr41xx_giu.h"

#include <utility>

namespace cerfcore {

void Vr41xxGiu::RegisterAlternateFunction(int first_gpio, int last_gpio, AlternateFunction fn) {
    alternates_.push_back({first_gpio, last_gpio, std::move(fn)});
}

void Vr41xxGiu::RegisterOutputDriveListener(std::function<void()> fn) {
    drive_listeners_.Add(std::move(fn));
}

void Vr41xxGiu::NotifyOutputDriveChanged() {
    drive_listeners_.Call();
}

void Vr41xxGiu::RegisterPinDeviceState(std::function<void(StateWriter&)> save,
                                       std::function<void(StateReader&)> restore) {
    pin_device_states_.push_back({std::move(save), std::move(restore)});
}

std::optional<PinDrive> Vr41xxGiu::AlternateDrive(int gpio) const {
    for (const Alternate& a : alternates_) {
        if (gpio < a.first || gpio > a.last) continue;
        if (const auto drive = a.fn(gpio)) return drive;
    }
    return std::nullopt;
}

void Vr41xxGiu::SavePinDevices(StateWriter& w) const {
    for (const PinDeviceState& d : pin_device_states_) d.save(w);
}

void Vr41xxGiu::RestorePinDevices(StateReader& r) const {
    for (const PinDeviceState& d : pin_device_states_) d.restore(r);
}

}

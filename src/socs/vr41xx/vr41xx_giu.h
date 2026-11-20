#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/pin_drive.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace cerfcore {

class StateWriter;
class StateReader;

class Vr41xxGiu : public Peripheral {
public:
    static constexpr int kDcdPin = 15;

    using Peripheral::Peripheral;

    virtual void SetPinLevel(int pin, bool level) = 0;
    virtual void ReleasePin(int pin) = 0;

    virtual bool GetPinLevel(int pin) = 0;

    using AlternateFunction = std::function<std::optional<PinDrive>(int gpio)>;
    void RegisterAlternateFunction(int first_gpio, int last_gpio, AlternateFunction fn);
    virtual PinDrive OutputDrive(int gpio) = 0;
    void RegisterOutputDriveListener(std::function<void()> fn);
    void NotifyOutputDriveChanged();

    void RegisterPinDeviceState(std::function<void(StateWriter&)> save,
                                std::function<void(StateReader&)> restore);

protected:
    std::optional<PinDrive> AlternateDrive(int gpio) const;
    void SavePinDevices(StateWriter& w) const;
    void RestorePinDevices(StateReader& r) const;

private:
    struct Alternate {
        int               first;
        int               last;
        AlternateFunction fn;
    };

    struct PinDeviceState {
        std::function<void(StateWriter&)> save;
        std::function<void(StateReader&)> restore;
    };

    std::vector<Alternate>      alternates_;
    std::vector<PinDeviceState> pin_device_states_;
    CallbackList<>              drive_listeners_;
};

}

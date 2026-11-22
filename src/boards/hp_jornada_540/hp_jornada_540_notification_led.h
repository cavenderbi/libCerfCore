#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_indicators.h>
#include "../../execution/guest_cycle_clock.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace cerfcore {

class HpJornada540Battery;
class HpJornada540Buttons;
class StateReader;
class StateWriter;

class HpJornada540NotificationLed : public Service, public MachineLamp {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void     Start();
    void     Stop();
    void     WriteBlinkCount(uint16_t count);
    uint16_t BlinkCount() const;
    bool     Blinking() const;
    void     SetDisplayOff(bool off);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

    std::string ObjectId() const override { return "notification_led"; }
    static constexpr char kObjectName[] = "Notification LED";
    std::string ObjectName() const override { return kObjectName; }
    LampState   Lamp() const override;

private:
    class Button final : public MachineButton {
    public:
        Button(HpJornada540NotificationLed& owner, CerfEmulator& emu) : MachineButton(emu), owner_(owner) {}
        std::string ObjectId() const override { return "button"; }
        static constexpr char kObjectName[] = "Notification button";
        std::string ObjectName() const override { return kObjectName; }

    private:
        bool Pressed() override {
            owner_.DriveButton(true);
            return true;
        }
        void Released() override { owner_.DriveButton(false); }

        HpJornada540NotificationLed& owner_;
    };

    void                     DriveButton(bool pressed);
    void                     ArmEndLocked();
    void                     End();
    std::optional<ObjectRgb> ChargeColor() const;

    HpJornada540Buttons*    buttons_   = nullptr;
    Button                  button_{*this, emu_};
    HpJornada540Battery*    battery_   = nullptr;
    GuestCycleClock*        clock_     = nullptr;
    GuestCycleClock::Event* end_event_ = nullptr;
    mutable std::mutex      mtx_;
    bool                    blinking_    = false;
    bool                    display_off_ = false;
    uint16_t                count_       = 0u;
    int64_t                 end_ns_   = 0;
};

}

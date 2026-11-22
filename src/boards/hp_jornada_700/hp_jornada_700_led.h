#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_indicators.h>

#include <mutex>

namespace cerfcore {

class HpJornada700Led : public Service, public MachineLamp {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    enum class State { Off, On, Blink };

    void    SetState(State s);
    uint8_t StatusByte() const;

    std::string ObjectId() const override { return "notification_led"; }
    static constexpr char kObjectName[] = "Notification LED";
    std::string ObjectName() const override { return kObjectName; }
    LampState   Lamp() const override;

private:
    class Button final : public MachineButton {
    public:
        Button(HpJornada700Led& owner, CerfEmulator& emu) : MachineButton(emu), owner_(owner) {}
        std::string ObjectId() const override { return "button"; }
        static constexpr char kObjectName[] = "Notification button";
        std::string ObjectName() const override { return kObjectName; }

    private:
        bool Pressed() override {
            owner_.DriveButton(true);
            return true;
        }
        void Released() override { owner_.DriveButton(false); }

        HpJornada700Led& owner_;
    };

    void  DriveButton(bool pressed);
    void  DriveButtonLine(bool pressed);
    State CurrentState() const;

    mutable std::mutex state_mutex_;
    State              state_ = State::Off;
    Button             button_{*this, emu_};
};

}

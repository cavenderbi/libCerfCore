#pragma once

#include "machine_object.h"
#include "published_state.h"

#include <cstdint>
#include <optional>
#include <string>

namespace cerfcore {

class CerfEmulator;

struct ObjectRgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    bool operator==(const ObjectRgb&) const = default;
};

struct LampState {
    bool                     lit      = false;
    bool                     blinking = false;
    ObjectRgb                color;
    uint32_t                 blink_on_ms  = 0;
    uint32_t                 blink_off_ms = 0;
    std::optional<ObjectRgb> between_blinks = std::nullopt;

    bool operator==(const LampState&) const = default;
};

class MachineLamp : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kLamp; }

    virtual LampState Lamp() const = 0;

protected:
    void Refresh();

private:
    void Republish() override { Refresh(); }

    PublishedState<LampState> published_;
};

class MachineVibrator : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kVibrator; }

    virtual bool Vibrating() const = 0;

protected:
    void Refresh();

private:
    void Republish() override { Refresh(); }

    PublishedState<bool> published_;
};

class MachineButton : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kButton; }

    void Press();

protected:
    explicit MachineButton(CerfEmulator& emu) : emu_(emu) {}

    virtual bool Pressed() = 0;

    CerfEmulator& emu_;
};

class MachineCharacterDisplay : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kCharacterDisplay; }

    virtual std::string Characters() const = 0;

protected:
    void Refresh();

private:
    void Republish() override { Refresh(); }

    PublishedState<std::string> published_;
};

}

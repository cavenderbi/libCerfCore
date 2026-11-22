#pragma once

#include "machine_object.h"
#include "published_state.h"

#include <atomic>
#include <cstdint>
#include <mutex>
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
    explicit MachineVibrator(CerfEmulator& emu) : emu_(emu) {}

    void Refresh();

private:
    void Republish() override { Refresh(); }

    CerfEmulator&        emu_;
    PublishedState<bool> published_;
};

class MachineButton : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kButton; }

    void Down();
    void Up();

    virtual bool Momentary() const { return false; }

    void RestoreObjectState(StateReader&) final;

protected:
    explicit MachineButton(CerfEmulator& emu) : emu_(emu) {}

    virtual bool Pressed()  = 0;
    virtual void Released() = 0;

    CerfEmulator& emu_;

private:
    void ForgetHoldsAfterRestoreLocked();

    std::mutex            mtx_;
    uint32_t              holders_       = 0;
    uint64_t              seen_restores_ = 0;
    std::atomic<uint64_t> restores_{0};
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

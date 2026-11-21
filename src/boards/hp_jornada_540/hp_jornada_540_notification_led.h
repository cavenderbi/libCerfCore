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

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

    std::string ObjectId() const override { return "notification_led"; }
    static constexpr char kObjectName[] = "Notification LED";
    std::string ObjectName() const override { return kObjectName; }
    LampState   Lamp() const override;

private:
    void                     ArmEndLocked();
    void                     End();
    std::optional<ObjectRgb> ChargeColor() const;

    HpJornada540Battery*    battery_   = nullptr;
    GuestCycleClock*        clock_     = nullptr;
    GuestCycleClock::Event* end_event_ = nullptr;
    mutable std::mutex      mtx_;
    bool                    blinking_ = false;
    uint16_t                count_    = 0u;
    int64_t                 end_ns_   = 0;
};

}

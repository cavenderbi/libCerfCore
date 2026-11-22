#pragma once

#include "sh77xx_scif2_serial_port_wiring.h"

#include <cstdint>

namespace cerfcore {

class Sh77xxPortWiring;

class HpSh3SerialPortWiring : public Sh77xxScif2SerialPortWiring {
public:
    using Sh77xxScif2SerialPortWiring::Sh77xxScif2SerialPortWiring;

    void OnReady() override;

protected:
    enum class ActiveLevel : uint8_t { kLow, kHigh };

    struct ControlPin {
        uint32_t    port  = 0u;
        uint8_t     mask  = 0u;
        ActiveLevel level = ActiveLevel::kLow;
    };

    struct ControlPins {
        ControlPin driver_power;
        ControlPin dtr;
        ControlPin rts;
    };

    virtual ControlPins Pins() const = 0;

private:
    void TrackNets();
    bool Asserted(const ControlPin& pin) const;

    Sh77xxPortWiring* ports_ = nullptr;
};

}

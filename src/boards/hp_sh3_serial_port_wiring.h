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
    struct ActiveLowPin {
        uint32_t port = 0u;
        uint8_t  mask = 0u;
    };

    struct ControlPins {
        ActiveLowPin driver_power;
        ActiveLowPin dtr;
        ActiveLowPin rts;
    };

    virtual ControlPins Pins() const = 0;

private:
    void TrackNets();
    bool Asserted(const ActiveLowPin& pin) const;

    Sh77xxPortWiring* ports_ = nullptr;
};

}

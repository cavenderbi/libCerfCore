#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/serial/serial_line.h"

#include <cstdint>

namespace cerfcore {

class UartEndpoint;

class Vr41xxDsiu : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual void                   AttachEndpoint(UartEndpoint* endpoint) = 0;
    virtual SerialLine::LineConfig TransmitFormat() = 0;
    virtual void ReceiveFromLine(uint8_t data, const SerialLine::LineConfig& sent) = 0;
};

}

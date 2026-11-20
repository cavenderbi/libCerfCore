#pragma once

#include "sh77xx_serial_lines_wiring.h"

namespace cerfcore {

class Sh77xxScif2SerialPortWiring : public Sh77xxSerialLinesWiring {
public:
    using Sh77xxSerialLinesWiring::Sh77xxSerialLinesWiring;

    std::optional<ObjectIdentity> Cradle(uint32_t channel_base) const override;
    MachineObject*                ShareLine(uint32_t channel_base, SerialLine& line) override;
};

}

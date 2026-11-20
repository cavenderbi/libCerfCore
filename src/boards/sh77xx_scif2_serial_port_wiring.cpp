#include "sh77xx_scif2_serial_port_wiring.h"

#include "../socs/sh77xx/sh77xx_scif.h"

namespace cerfcore {

namespace {

constexpr char kSerialPortId[]    = "serial_port";
constexpr char kSerialPortLabel[] = "Serial port";

}

std::optional<ObjectIdentity> Sh77xxScif2SerialPortWiring::Cradle(uint32_t channel_base) const {
    if (channel_base != kSh77xxScif2Base) return std::nullopt;
    return ObjectIdentity{kSerialPortId, kSerialPortLabel};
}

MachineObject* Sh77xxScif2SerialPortWiring::ShareLine(uint32_t, SerialLine&) { return nullptr; }

}

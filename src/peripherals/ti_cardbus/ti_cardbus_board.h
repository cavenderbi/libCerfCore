#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class TiCardbusBoard : public Service {
public:
    using Service::Service;

    virtual uint8_t  PciDevice() const = 0;
    virtual bool     SerialEepromPresent() const = 0;
    virtual uint32_t MultifunctionRoutingAtPowerOn() const = 0;
    virtual uint8_t  GeneralPurposeInputLevels() const = 0;
    virtual void     SetPciInterruptLevel(uint8_t interrupt_pin, bool serialized, bool level) = 0;
    virtual void     SetIsaIrqLevels(uint8_t function, bool serialized, uint16_t driven, uint16_t irqs) = 0;
};

}

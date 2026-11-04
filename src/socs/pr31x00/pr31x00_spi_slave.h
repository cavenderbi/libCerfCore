#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

class Pr31x00SpiSlave : public Service {
public:
    using Service::Service;

    virtual bool    SpiRxHasByte() = 0;
    virtual uint8_t SpiRxReadByte() = 0;

    /* A TXDATA write clocks a byte from the SPI module to the slave (§14.3.2). */
    virtual void    SpiTxByte(uint8_t byte) = 0;

    virtual void SaveState(StateWriter&) {}
    virtual void RestoreState(StateReader&) {}
};

}

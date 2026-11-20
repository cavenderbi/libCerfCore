#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

class I2cSlave : public Service {
public:
    using Service::Service;
    ~I2cSlave() override = default;

    virtual void SaveState(StateWriter&) {}
    virtual void RestoreState(StateReader&) {}
    virtual void PostRestore() {}

    virtual bool MatchesAddress(uint8_t slave_addr) const = 0;

    virtual void TxnStart(uint8_t slave_addr) = 0;

    virtual bool TxnWriteByte(uint8_t slave_addr, uint8_t byte) = 0;

    virtual uint8_t TxnReadByte(uint8_t slave_addr) = 0;

    virtual void BusStart() {}
    virtual void BusStop() {}
};

}

#pragma once

#include <cstdint>

namespace cerfcore {

class I2cSlave;
class StateWriter;
class StateReader;

class IntelI2cUnit {
public:
    static constexpr uint32_t kIbmrIdle        = 0x3u;
    static constexpr uint32_t kIcrTransferByte = 1u << 3;

    uint32_t Icr() const { return icr_; }
    uint32_t Isr() const;
    uint32_t Idbr() const { return idbr_; }
    uint32_t Isar() const { return isar_; }

    void WriteIcr(uint32_t value, I2cSlave* slave);
    void WriteIsr(uint32_t value);
    void WriteIdbr(uint32_t value) { idbr_ = value & 0xFFu; }
    void WriteIsar(uint32_t value) { isar_ = value & 0x7Fu; }

    void Reset(I2cSlave* slave);
    bool InterruptActive() const;
    void SequencerTransmitted(uint8_t last_byte, bool stop);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    void TransferByte(uint32_t control, I2cSlave* slave);
    void StartAddress(I2cSlave* slave);
    void NoAcknowledge(I2cSlave* slave);
    void StopTransaction(I2cSlave* slave);

    uint32_t icr_      = 0;
    uint32_t isr_      = 0;
    uint32_t idbr_     = 0;
    uint32_t isar_     = 0;
    uint8_t  address_  = 0;
    bool     selected_ = false;
    bool     reading_  = false;
};

}

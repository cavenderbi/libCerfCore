#pragma once

#include "../intel_i2c_unit.h"

#include <cstdint>

namespace cerfcore {

class I2cSlave;
class StateWriter;
class StateReader;

class Pxa27xI2cUnit {
public:
    static constexpr uint32_t kIBMR = 0x00u;
    static constexpr uint32_t kIDBR = 0x08u;
    static constexpr uint32_t kICR  = 0x10u;
    static constexpr uint32_t kISR  = 0x18u;
    static constexpr uint32_t kISAR = 0x20u;

    bool Read(uint32_t off, uint32_t& value) const;
    bool Write(uint32_t off, uint32_t value, I2cSlave* slave);
    void Reset(I2cSlave* slave) { unit_.Reset(slave); }
    bool InterruptActive() const { return unit_.InterruptActive(); }
    void SequencerTransmitted(uint8_t last_byte, bool stop) { unit_.SequencerTransmitted(last_byte, stop); }

    void SaveState(StateWriter& w) const { unit_.SaveState(w); }
    void RestoreState(StateReader& r) { unit_.RestoreState(r); }

private:
    IntelI2cUnit unit_;
};

}

#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include "pxa27x_i2c_unit.h"

#include <cstdint>

namespace cerfcore {

class Pxa27xPowerI2c : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override { return 0x40F00180u; }
    uint32_t MmioSize() const override { return 0x00000024u; }

    uint32_t ReadWord (uint32_t addr) override;
    void     WriteWord(uint32_t addr, uint32_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override { UpdateIrq(); }

    void ResetRegisters();
    void SequencerTransmitted(uint8_t last_byte, bool stop);

private:
    void UpdateIrq();

    Pxa27xI2cUnit unit_;
};

}

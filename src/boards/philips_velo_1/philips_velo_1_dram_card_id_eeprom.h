#pragma once

#include "../../socs/i2c_slave.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cerfcore {

class PhilipsVelo1DramCardIdEeprom : public I2cSlave {
public:
    using I2cSlave::I2cSlave;

    bool ShouldRegister() override;
    void OnReady() override;

    bool    MatchesAddress(uint8_t slave_addr) const override;
    void    TxnStart(uint8_t slave_addr) override;
    void    TxnWriteByte(uint8_t slave_addr, uint8_t byte) override;
    uint8_t TxnReadByte(uint8_t slave_addr) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

private:
    static constexpr size_t kIdRomBytes = 256u;

    std::array<uint8_t, kIdRomBytes> id_rom_{};
    uint8_t word_addr_         = 0;
    bool    word_addr_pending_ = false;
};

}

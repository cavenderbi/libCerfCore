#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace cerfcore {

class I2cSlave;
class StateWriter;
class StateReader;

class I2cBitBangTarget {
public:
    using SdaDrive  = std::function<void(bool released)>;
    using Unmatched = std::function<void(uint8_t addr, bool read)>;

    I2cBitBangTarget(SdaDrive drive, std::vector<I2cSlave*> slaves, Unmatched unmatched);

    void OnLines(bool scl, bool sda);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    enum class Phase : uint8_t { kIdle, kAddress, kWrite, kRead };
    static constexpr uint8_t kNoSlave = 0xFFu;

    void Start();
    void Stop();
    void ClockRise();
    void ClockFall();
    bool AcknowledgeByte();
    void AfterAck();
    void LoadReadByte();
    I2cSlave* Find(uint8_t addr) const;

    SdaDrive               drive_;
    std::vector<I2cSlave*> slaves_;
    Unmatched              unmatched_;
    I2cSlave*              slave_ = nullptr;

    Phase   phase_       = Phase::kIdle;
    uint8_t shift_       = 0;
    uint8_t bit_         = 0;
    uint8_t addr_        = 0;
    bool    read_        = false;
    bool    in_ack_      = false;
    bool    master_nack_ = false;
    bool    scl_         = true;
    bool    sda_         = true;
};

}

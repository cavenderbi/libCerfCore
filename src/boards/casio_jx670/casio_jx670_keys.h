#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

namespace cerfcore {

class StateWriter;
class StateReader;

class CasioJx670Keys {
public:
    static constexpr uint16_t kPowerButton = 0x8000u;

    void Init(std::function<void()> on_status_change, std::function<void(bool)> on_power_line);
    void PowerOnRegisters();

    bool TryReadHalf (uint32_t off, uint16_t& out) const;
    bool TryWriteHalf(uint32_t off, uint16_t  value);

    void SetHeld(uint16_t bits, bool down);
    void LatchWake(uint16_t bits);
    bool IrqPending() const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    uint16_t ArmedMask() const;
    void PublishPowerLine();

    std::function<void()>     on_status_change_;
    std::function<void(bool)> on_power_line_;

    std::atomic<uint16_t> enable_{0};
    std::atomic<uint16_t> held_{0};
    std::atomic<uint16_t> latch_{0};
    std::atomic<uint16_t> cfg_[16] = {};
    bool                  power_line_ = false;
};

}

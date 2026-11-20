#pragma once

#include <cerfcore/state/state_stream.h>
#include "pin_drive.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace cerfcore {

template <uint32_t kPorts, uint32_t kPins>
class PinNetHold {
public:
    PinNetHold() = default;
    explicit PinNetHold(const std::array<uint8_t, kPorts>& board_pull_ups) : board_pull_ups_(board_pull_ups) {}

    template <class Chip>
    void Track(const Chip& chip) {
        for (uint32_t port = 0; port < kPorts; ++port) {
            uint8_t nets = nets_[port].load(std::memory_order_acquire);
            for (uint32_t pin = 0; pin < kPins; ++pin) {
                const uint8_t bit = static_cast<uint8_t>(1u << pin);
                switch (chip.Drive(port, pin)) {
                    case PinDrive::kLow:      nets = static_cast<uint8_t>(nets & ~bit); break;
                    case PinDrive::kHigh:
                    case PinDrive::kPulledUp: nets = static_cast<uint8_t>(nets | bit); break;
                    case PinDrive::kUndriven:
                        if ((board_pull_ups_[port] & bit) != 0u) nets = static_cast<uint8_t>(nets | bit);
                        break;
                    case PinDrive::kFunction: break;
                }
            }
            nets_[port].store(nets, std::memory_order_release);
        }
    }

    uint8_t Levels(uint32_t port) const { return nets_[port].load(std::memory_order_acquire); }

    void SaveState(StateWriter& w) const {
        for (uint32_t port = 0; port < kPorts; ++port) {
            w.Write<uint8_t>("net", nets_[port].load(std::memory_order_acquire));
        }
    }

    void RestoreState(StateReader& r) {
        for (uint32_t port = 0; port < kPorts; ++port) {
            uint8_t nets = 0u;
            r.Read("net", nets);
            nets_[port].store(nets, std::memory_order_release);
        }
    }

private:
    std::array<uint8_t, kPorts> board_pull_ups_{};
    std::atomic<uint8_t>        nets_[kPorts]{};
};

}

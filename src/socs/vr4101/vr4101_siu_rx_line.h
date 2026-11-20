#pragma once

#include <cerfcore/core/service.h>
#include "../timed_uart_rx_line.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace cerfcore {

class StateReader;
class StateWriter;

class Vr4101SiuRxLine : public Service {
public:
    struct Timing {
        uint32_t baud             = 0;
        uint32_t bits_before_stop = 0;
        uint32_t frame_bits       = 0;

        bool operator==(const Timing&) const = default;
    };
    using LandFn = std::function<void(uint8_t)>;
    using IdleFn = std::function<void()>;

    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void Attach(LandFn land, IdleFn on_idle);
    bool Push(const uint8_t* data, size_t n) { return rx_.Push(data, n); }
    bool Idle() const;
    void SetTiming(const Timing& timing);
    void Settle();
    void Reset();

    void Save(StateWriter& w);
    void Restore(StateReader& r);

private:
    static TimedUartRxLine::Timing RxTiming(const Timing& t);

    void OnHostRequest();
    void OnRate();
    void Land(std::vector<uint8_t>& landed, bool& idle);
    void Finish(const std::vector<uint8_t>& landed, bool idle);

    mutable std::mutex mu_;
    Timing             timing_;
    TimedUartRxLine    rx_{emu_, "SIU"};

    LandFn land_;
    IdleFn on_idle_;
};

}

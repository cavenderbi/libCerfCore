#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

namespace cerfcore {

class DeferredJobs;
class Sh77xxScifPins;
struct Sh77xxScifChannel;

class Sh77xxScifModemLines {
public:
    static bool Enabled(uint8_t fcr);
    static bool RtsHigh(uint8_t fcr, bool receive_full);

    void Attach(Sh77xxScifPins& pins, DeferredJobs& jobs, const Sh77xxScifChannel& channel,
                std::function<bool()> rts_level);

    bool HoldsTransmit(uint8_t fcr) const;
    void RefreshRts();
    void PostRtsRefresh();

private:
    Sh77xxScifPins*          pins_    = nullptr;
    DeferredJobs*            jobs_    = nullptr;
    const Sh77xxScifChannel* channel_ = nullptr;
    std::function<bool()>    rts_level_;
    std::atomic<bool>        rts_high_{false};
};

}

#include "sh77xx_scif_modem_lines.h"

#include "../../core/deferred_jobs.h"
#include "sh77xx_scif_pins.h"

#include <utility>

namespace cerfcore {

namespace {

constexpr uint8_t kFifoMce = 0x08u;

}

bool Sh77xxScifModemLines::Enabled(uint8_t fcr) { return (fcr & kFifoMce) != 0u; }

bool Sh77xxScifModemLines::RtsHigh(uint8_t fcr, bool receive_full) { return Enabled(fcr) && receive_full; }

void Sh77xxScifModemLines::Attach(Sh77xxScifPins& pins, DeferredJobs& jobs, const Sh77xxScifChannel& channel,
                                  std::function<bool()> rts_level) {
    pins_      = &pins;
    jobs_      = &jobs;
    channel_   = &channel;
    rts_level_ = std::move(rts_level);
    pins_->DriveRts(channel, false);
}

bool Sh77xxScifModemLines::HoldsTransmit(uint8_t fcr) const { return Enabled(fcr) && pins_->CtsHigh(*channel_); }

void Sh77xxScifModemLines::RefreshRts() {
    const bool high = rts_level_();
    if (rts_high_.exchange(high, std::memory_order_acq_rel) != high) pins_->DriveRts(*channel_, high);
}

void Sh77xxScifModemLines::PostRtsRefresh() {
    if (rts_level_() != rts_high_.load(std::memory_order_acquire)) jobs_->PostToGuestThread([this] { RefreshRts(); });
}

}

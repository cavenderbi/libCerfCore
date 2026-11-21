#include "sh77xx_refresh_timer.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kCksShift          = 3u;
constexpr uint16_t kCksMask           = 7u;
constexpr uint16_t kLmts512           = 0x0001u;
constexpr uint32_t kCksDivisorShift[] = {0u, 2u, 4u, 6u, 8u, 10u, 11u, 12u};
constexpr uint32_t kCounterSpan       = 256u;
constexpr uint32_t kRfcrMask          = 0x03FFu;
constexpr uint32_t kLimit1024         = 1024u;
constexpr uint32_t kLimit512          = 512u;

}

void Sh77xxRefreshTimer::Attach() { ckio_.Attach([] {}); }

void Sh77xxRefreshTimer::PowerOnReset() {
    shift_         = 0u;
    lmts512_       = false;
    rtcor_         = 0u;
    count_         = 0u;
    anchor_        = 0u;
    matches_       = 0u;
    count_known_   = true;
    matches_known_ = true;
}

uint64_t Sh77xxRefreshTimer::InputTicks() const { return ckio_.DividedTicks(shift_); }

void Sh77xxRefreshTimer::Fold() {
    if (shift_ == 0u) return;
    const uint64_t now = InputTicks();
    const uint64_t n   = now - anchor_;
    anchor_            = now;
    if (n == 0u) return;
    if (!count_known_ || rtcor_ == 0u || count_ == rtcor_) {
        count_known_   = false;
        matches_known_ = false;
        return;
    }
    const uint64_t first = count_ < rtcor_ ? rtcor_ - count_ : kCounterSpan - count_ + rtcor_;
    if (n < first) {
        count_ = static_cast<uint8_t>(count_ + n);
        return;
    }
    const uint64_t after = n - first;
    CountRefreshRequests(1u + after / rtcor_);
    count_ = static_cast<uint8_t>(after % rtcor_);
}

void Sh77xxRefreshTimer::CountRefreshRequests(uint64_t requests) {
    const uint32_t limit         = lmts512_ ? kLimit512 : kLimit1024;
    const uint64_t to_first_clear = matches_ < limit ? limit - matches_ : 1u;
    if (requests < to_first_clear) {
        matches_ = static_cast<uint32_t>(matches_ + requests);
        return;
    }
    matches_ = static_cast<uint32_t>((requests - to_first_clear) % limit);
}

void Sh77xxRefreshTimer::WriteControl(uint16_t rtcsr) {
    Fold();
    shift_   = kCksDivisorShift[(rtcsr >> kCksShift) & kCksMask];
    lmts512_ = (rtcsr & kLmts512) != 0u;
    anchor_  = InputTicks();
}

void Sh77xxRefreshTimer::WriteCount(uint16_t rtcnt) {
    Fold();
    count_       = static_cast<uint8_t>(rtcnt);
    count_known_ = true;
}

void Sh77xxRefreshTimer::WriteConstant(uint16_t rtcor) {
    Fold();
    rtcor_ = static_cast<uint8_t>(rtcor);
}

void Sh77xxRefreshTimer::WriteRefreshCount(uint16_t rfcr) {
    Fold();
    matches_       = rfcr & kRfcrMask;
    matches_known_ = true;
}

uint16_t Sh77xxRefreshTimer::RefreshCount() {
    Fold();
    if (!matches_known_) {
        emu_.Get<Fatal>().Die("Sh77xxRefreshTimer: RFCR read after counting with RTCOR 0x%02X or an unknown RTCNT",
                              rtcor_);
    }
    return static_cast<uint16_t>(matches_);
}

void Sh77xxRefreshTimer::Save(StateWriter& w) const {
    w.Write("refresh_shift", shift_);
    w.Write("refresh_lmts512", lmts512_);
    w.Write("refresh_rtcor", rtcor_);
    w.Write("refresh_count", count_);
    w.Write("refresh_anchor", anchor_);
    w.Write("refresh_matches", matches_);
    w.Write("refresh_count_known", count_known_);
    w.Write("refresh_matches_known", matches_known_);
    ckio_.Save(w);
}

void Sh77xxRefreshTimer::Restore(StateReader& r) {
    r.Read("refresh_shift", shift_);
    r.Read("refresh_lmts512", lmts512_);
    r.Read("refresh_rtcor", rtcor_);
    r.Read("refresh_count", count_);
    r.Read("refresh_anchor", anchor_);
    r.Read("refresh_matches", matches_);
    r.Read("refresh_count_known", count_known_);
    r.Read("refresh_matches_known", matches_known_);
    ckio_.Restore(r);
}

void Sh77xxRefreshTimer::PostRestore() { ckio_.PostRestore(); }

}

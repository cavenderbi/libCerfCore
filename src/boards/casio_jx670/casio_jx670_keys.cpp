#include "casio_jx670_keys.h"

#include <cerfcore/state/state_stream.h>

#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kOffEnable    = 0xA040u;
constexpr uint32_t kOffWakeLatch = 0xA042u;
constexpr uint32_t kOffHeld      = 0xA044u;
constexpr uint32_t kOffCfgLo     = 0xA060u;
constexpr uint32_t kOffCfgHi     = 0xA07Eu;
constexpr uint16_t kButtonMask   = 0x9EFFu;
constexpr uint16_t kCfgArm       = 0x0001u;
constexpr uint16_t kCfgIdle      = 0x0030u;
constexpr uint16_t kCfgScanBits  = 0x003Cu;

bool IsCfg(uint32_t off) {
    return off >= kOffCfgLo && off <= kOffCfgHi && (off & 1u) == 0u;
}

}

void CasioJx670Keys::Init(std::function<void()> on_status_change,
                          std::function<void(bool)> on_power_line) {
    on_status_change_ = std::move(on_status_change);
    on_power_line_    = std::move(on_power_line);
}

void CasioJx670Keys::PowerOnRegisters() {
    enable_.store(0, std::memory_order_release);
    latch_.store(0, std::memory_order_release);
    for (std::atomic<uint16_t>& c : cfg_) c.store(0, std::memory_order_release);
}

bool CasioJx670Keys::TryReadHalf(uint32_t off, uint16_t& out) const {
    if (IsCfg(off)) {
        out = cfg_[(off - kOffCfgLo) / 2u].load(std::memory_order_acquire);
        return true;
    }
    switch (off) {
        case kOffEnable:    out = enable_.load(std::memory_order_acquire); return true;
        case kOffWakeLatch: out = latch_.load(std::memory_order_acquire);  return true;
        case kOffHeld:      out = held_.load(std::memory_order_acquire);   return true;
        default:            return false;
    }
}

bool CasioJx670Keys::TryWriteHalf(uint32_t off, uint16_t value) {
    if (IsCfg(off)) {
        const uint32_t bit = (off - kOffCfgLo) / 2u;
        const uint16_t cur = cfg_[bit].load(std::memory_order_acquire);
        if (value != kCfgIdle && value != kCfgArm && value != static_cast<uint16_t>(cur | kCfgScanBits) &&
            value != static_cast<uint16_t>(cur | kCfgIdle))
            return false;
        cfg_[bit].store(value, std::memory_order_release);
        if ((value & kCfgArm) != 0u)
            latch_.fetch_and(static_cast<uint16_t>(~(1u << bit)), std::memory_order_acq_rel);
        return true;
    }
    if (off != kOffEnable) return false;
    enable_.store(value, std::memory_order_release);
    on_status_change_();
    return true;
}

void CasioJx670Keys::SetHeld(uint16_t bits, bool down) {
    if (down) {
        const uint16_t pressed = static_cast<uint16_t>(bits & ~held_.fetch_or(bits, std::memory_order_acq_rel));
        LatchWake(pressed);
    } else {
        held_.fetch_and(static_cast<uint16_t>(~bits), std::memory_order_acq_rel);
    }
    on_status_change_();
    PublishPowerLine();
}

void CasioJx670Keys::LatchWake(uint16_t bits) {
    latch_.fetch_or(static_cast<uint16_t>(bits & ArmedMask()), std::memory_order_acq_rel);
}

bool CasioJx670Keys::IrqPending() const {
    return (held_.load(std::memory_order_acquire) & enable_.load(std::memory_order_acquire) &
            kButtonMask) != 0u;
}

uint16_t CasioJx670Keys::ArmedMask() const {
    uint16_t mask = 0;
    for (uint32_t bit = 0; bit < 16u; ++bit) {
        if ((cfg_[bit].load(std::memory_order_acquire) & kCfgArm) != 0u)
            mask = static_cast<uint16_t>(mask | (1u << bit));
    }
    return mask;
}

void CasioJx670Keys::PublishPowerLine() {
    const bool line = (held_.load(std::memory_order_acquire) & kPowerButton) != 0u;
    if (line == power_line_) return;
    power_line_ = line;
    on_power_line_(line);
}

void CasioJx670Keys::SaveState(StateWriter& w) const {
    w.Write("key_enable", enable_.load(std::memory_order_acquire));
    w.Write("key_wake_latch", latch_.load(std::memory_order_acquire));
    for (const std::atomic<uint16_t>& c : cfg_) w.Write("key_cfg", c.load(std::memory_order_acquire));
}

void CasioJx670Keys::RestoreState(StateReader& r) {
    uint16_t v = 0;
    r.Read("key_enable", v);
    enable_.store(v, std::memory_order_release);
    r.Read("key_wake_latch", v);
    latch_.store(v, std::memory_order_release);
    for (std::atomic<uint16_t>& c : cfg_) {
        r.Read("key_cfg", v);
        c.store(v, std::memory_order_release);
    }
    held_.store(0, std::memory_order_release);
    power_line_ = false;
}

}

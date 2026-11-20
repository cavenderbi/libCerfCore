#include "vr4101_siu_rx_line.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>
#include "vr4101_id.h"

#include <utility>

namespace cerfcore {

bool Vr4101SiuRxLine::ShouldRegister() { return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4101; }

void Vr4101SiuRxLine::OnReady() {
    rx_.Attach([this] { Settle(); }, [this] { OnHostRequest(); }, [this] { OnRate(); });
}

void Vr4101SiuRxLine::Attach(LandFn land, IdleFn on_idle) {
    land_    = std::move(land);
    on_idle_ = std::move(on_idle);
}

bool Vr4101SiuRxLine::Idle() const {
    std::lock_guard<std::mutex> lk(mu_);
    return rx_.Line().Idle();
}

void Vr4101SiuRxLine::SetTiming(const Timing& timing) {
    std::vector<uint8_t> landed;
    bool                 idle = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (timing == timing_) return;
        const uint64_t now = rx_.Now();
        if (rx_.Settle(now, true, [&landed](uint16_t entry) { landed.push_back(static_cast<uint8_t>(entry)); }) &&
            rx_.Line().Idle()) {
            idle = true;
        }
        const Timing old = timing_;
        timing_          = timing;
        if (rx_.Retime(RxTiming(old), RxTiming(timing), now, true)) idle = true;
    }
    Finish(landed, idle);
}

void Vr4101SiuRxLine::Settle() {
    std::vector<uint8_t> landed;
    bool                 idle = false;
    Land(landed, idle);
    Finish(landed, idle);
}

void Vr4101SiuRxLine::Reset() {
    bool dropped = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        dropped = !rx_.Line().Idle();
        rx_.Stop();
        timing_ = Timing{};
    }
    if (dropped) on_idle_();
}

void Vr4101SiuRxLine::Save(StateWriter& w) {
    std::lock_guard<std::mutex> lk(mu_);
    w.Write("rx_baud", timing_.baud);
    w.Write("rx_bits_before_stop", timing_.bits_before_stop);
    w.Write("rx_frame_bits", timing_.frame_bits);
    rx_.Save(w);
}

void Vr4101SiuRxLine::Restore(StateReader& r) {
    std::lock_guard<std::mutex> lk(mu_);
    r.Read("rx_baud", timing_.baud);
    r.Read("rx_bits_before_stop", timing_.bits_before_stop);
    r.Read("rx_frame_bits", timing_.frame_bits);
    rx_.Restore(r);
    rx_.Arm(rx_.Now(), true);
}

TimedUartRxLine::Timing Vr4101SiuRxLine::RxTiming(const Timing& t) {
    TimedUartRxLine::Timing timing;
    timing.clocked = t.baud != 0u;
    timing.bit     = GuestCycleClock::Rate{t.baud, 1u};
    timing.frame   = UartRxLine::FirstStopCentre(t.bits_before_stop, t.frame_bits);
    return timing;
}

void Vr4101SiuRxLine::OnHostRequest() {
    bool idle = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        idle = rx_.Intake(RxTiming(timing_), rx_.Now(), true);
    }
    if (idle) on_idle_();
}

void Vr4101SiuRxLine::OnRate() {
    std::lock_guard<std::mutex> lk(mu_);
    rx_.Rescale(rx_.Now(), true);
}

void Vr4101SiuRxLine::Land(std::vector<uint8_t>& landed, bool& idle) {
    std::lock_guard<std::mutex> lk(mu_);
    const bool any = rx_.Settle(rx_.Now(), true, [&landed](uint16_t entry) {
        landed.push_back(static_cast<uint8_t>(entry));
    });
    idle = any && rx_.Line().Idle();
}

void Vr4101SiuRxLine::Finish(const std::vector<uint8_t>& landed, bool idle) {
    for (const uint8_t byte : landed) land_(byte);
    if (idle) on_idle_();
}

REGISTER_SERVICE(Vr4101SiuRxLine);

}

#include "pr31x00_counter_int.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>
#include "pr31500_id.h"
#include "pr31700_id.h"

namespace cerfcore {

bool Pr31x00CounterInt::ShouldRegister() {
    const std::string_view soc = emu_.Get<BoardContext>().GetSocId();
    return soc == SocId::Pr31500 || soc == SocId::Pr31700;
}

void Pr31x00CounterInt::OnReady() {
    clock_ = &emu_.Get<GuestCycleClock>();
    intc_  = &emu_.Get<Pr31x00Intc>();
}

uint32_t Pr31x00CounterInt::Attach(uint32_t set, uint32_t bit, Pr31x00CounterTimeline& timeline) {
    const uint32_t port = static_cast<uint32_t>(ch_.size());
    ch_.push_back(std::make_unique<Channel>(*this, set, bit, timeline));
    intc_->RegisterTimedSource(set, bit, *ch_[port]);
    return port;
}

void Pr31x00CounterInt::Rearm(uint32_t port) { ch_[port]->Rearm(); }

void Pr31x00CounterInt::SaveState(uint32_t port, StateWriter& w) const { ch_[port]->Save(w); }

void Pr31x00CounterInt::RestoreState(uint32_t port, StateReader& r) { ch_[port]->Restore(r); }

Pr31x00CounterInt::Channel::Channel(Pr31x00CounterInt& owner, uint32_t set, uint32_t bit,
                                    Pr31x00CounterTimeline& timeline)
    : owner_(owner), set_(set), bit_(bit), timeline_(timeline) {
    event_ = owner_.clock_->Add([this] { Latch(); });
}

void Pr31x00CounterInt::Channel::Settle() { timeline_.Settle(); }

bool Pr31x00CounterInt::Channel::EdgeSinceClear() const {
    return !latched_ && timeline_.CountAt(owner_.clock_->Cycles()) > cleared_;
}

void Pr31x00CounterInt::Channel::OnLatched() {
    latched_ = true;
    owner_.clock_->Disarm(event_);
}

void Pr31x00CounterInt::Channel::OnStatusCleared() {
    latched_ = false;
    cleared_ = timeline_.CountAt(owner_.clock_->Cycles());
    Rearm();
}

void Pr31x00CounterInt::Channel::OnPathChanged(bool open) {
    open_ = open;
    Rearm();
}

void Pr31x00CounterInt::Channel::Rearm() {
    if (!open_ || latched_) {
        owner_.clock_->Disarm(event_);
        return;
    }
    const uint64_t now = owner_.clock_->Cycles();
    if (EdgeSinceClear()) {
        owner_.clock_->Arm(event_, now);
        return;
    }
    uint64_t at = 0;
    if (timeline_.NextCountCycle(now, at)) owner_.clock_->Arm(event_, at);
    else                                   owner_.clock_->Disarm(event_);
}

void Pr31x00CounterInt::Channel::Latch() {
    if (!EdgeSinceClear()) {
        Rearm();
        return;
    }
    OnLatched();
    owner_.intc_->SetPending(set_, bit_);
}

void Pr31x00CounterInt::Channel::Save(StateWriter& w) const {
    w.Write("cntint_cleared", cleared_);
    w.Write<uint8_t>("cntint_latched", latched_ ? 1u : 0u);
}

void Pr31x00CounterInt::Channel::Restore(StateReader& r) {
    uint8_t latched = 0;
    r.Read("cntint_cleared", cleared_);
    r.Read("cntint_latched", latched);
    latched_ = latched != 0u;
    owner_.clock_->Disarm(event_);
}

REGISTER_SERVICE(Pr31x00CounterInt);

}

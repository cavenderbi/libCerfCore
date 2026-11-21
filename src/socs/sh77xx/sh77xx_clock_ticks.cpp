#include "sh77xx_clock_ticks.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>
#include "sh77xx_cpg.h"
#include "sh77xx_frqcr.h"

#include <utility>

namespace cerfcore {

namespace {

struct TickFieldNames {
    const char* ticks;
    const char* phase;
};

constexpr TickFieldNames kFieldNames[] = {
    {"pclk_ticks", "pclk_phase"},
    {"ckio_ticks", "ckio_phase"},
};

const TickFieldNames& FieldNames(Sh77xxTickedClock clock) { return kFieldNames[static_cast<uint8_t>(clock)]; }

}

void Sh77xxClockTicks::Attach(std::function<void()> on_retime) {
    clock_     = &emu_.Get<GuestCycleClock>();
    frqcr_     = &emu_.Get<Sh77xxFrqcr>();
    cpg_       = &emu_.Get<Sh77xxCpg>();
    on_retime_ = std::move(on_retime);
    rate_      = SourceRate(frqcr_->Value());
    ticks_.SetRate(clock_->ClockRate(), rate_);
    ticks_.Start(clock_->Cycles());
    clock_->RegisterRateListener([this] { Retime(); });
    frqcr_->RegisterChangeListener([this](uint16_t, uint16_t to) {
        rate_ = SourceRate(to);
        Retime();
    });
}

GuestCycleClock::Rate Sh77xxClockTicks::SourceRate(uint16_t frqcr) const {
    return clock_kind_ == Sh77xxTickedClock::kPeripheral ? cpg_->PeripheralRate(frqcr) : cpg_->InternalCkioRate();
}

uint64_t Sh77xxClockTicks::Ticks() const { return ticks_.TicksAt(clock_->Cycles()); }

uint64_t Sh77xxClockTicks::CycleOfTick(uint64_t tick) const { return ticks_.CycleOfTick(tick); }

uint64_t Sh77xxClockTicks::DividedTicks(uint32_t shift) const { return Ticks() >> shift; }

uint64_t Sh77xxClockTicks::CycleOfDividedTick(uint64_t tick, uint32_t shift) const {
    return CycleOfTick(tick << shift);
}

void Sh77xxClockTicks::Retime() {
    ticks_.Rescale(clock_->Cycles(), clock_->ClockRate(), rate_);
    on_retime_();
}

void Sh77xxClockTicks::Save(StateWriter& w) const {
    const uint64_t now = clock_->Cycles();
    w.Write(FieldNames(clock_kind_).ticks, ticks_.TicksAt(now));
    ticks_.TickPhaseAt(now).Save(w, FieldNames(clock_kind_).phase);
}

void Sh77xxClockTicks::Restore(StateReader& r) {
    r.Read(FieldNames(clock_kind_).ticks, restored_ticks_);
    restored_phase_.Restore(r, FieldNames(clock_kind_).phase);
}

void Sh77xxClockTicks::PostRestore() {
    rate_ = SourceRate(frqcr_->Value());
    ticks_.SetRate(clock_->ClockRate(), rate_);
    ticks_.PlaceAtPhase(clock_->Cycles(), restored_ticks_, restored_phase_);
}

}

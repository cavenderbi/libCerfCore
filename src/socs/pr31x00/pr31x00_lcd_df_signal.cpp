#include "pr31x00_lcd_df_signal.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

void Pr31x00LcdDfSignal::Reset(uint32_t dfval) { counter_.PowerOnReset(dfval); }

void Pr31x00LcdDfSignal::Start(bool every_frame, uint64_t frame_lines) {
    counter_.Start(0u);
    frame_       = frame_lines;
    every_frame_ = every_frame;
    epoch_tick_  = 0u;
    epoch_flags_ = counter_.FlagTotalThrough(0u);
    from_start_  = true;
}

void Pr31x00LcdDfSignal::Restart(uint64_t from_tick, uint64_t frame_lines) {
    done_ += EpochToggles(from_tick);
    counter_.Move(from_tick, 0u);
    frame_       = frame_lines;
    epoch_tick_  = 0u;
    epoch_flags_ = counter_.FlagTotalThrough(0u);
    from_start_  = false;
}

void Pr31x00LcdDfSignal::Stop(uint64_t tick) {
    if (!counter_.Running()) return;
    done_ += EpochToggles(tick);
    counter_.Stop(tick);
    end_ = UINT64_MAX;
}

void Pr31x00LcdDfSignal::SetMode(uint64_t tick, bool every_frame) {
    if (every_frame == every_frame_) return;
    if (counter_.Running()) {
        done_       += EpochToggles(tick);
        epoch_tick_  = tick;
        epoch_flags_ = counter_.FlagTotalThrough(tick);
        from_start_  = false;
    }
    every_frame_ = every_frame;
}

void Pr31x00LcdDfSignal::SetDfVal(uint64_t tick, uint32_t dfval) { counter_.WriteConstant(tick, dfval); }

uint64_t Pr31x00LcdDfSignal::EpochToggles(uint64_t tick) const {
    if (!every_frame_) return counter_.FlagTotalThrough(tick) - epoch_flags_;
    const uint64_t lo = FirstFrameTick();
    const uint64_t hi = tick < end_ - 1u ? tick : end_ - 1u;
    if (hi < lo) return 0u;
    return FrameStartsThrough(hi) - (lo == 0u ? 0u : FrameStartsThrough(lo - 1u));
}

uint64_t Pr31x00LcdDfSignal::TogglesThrough(uint64_t tick) const {
    return counter_.Running() ? done_ + EpochToggles(tick) : done_;
}

bool Pr31x00LcdDfSignal::NextToggle(uint64_t tick, uint64_t& at) const {
    if (!counter_.Running()) return false;
    if (!every_frame_) {
        at = counter_.FlagAfter(tick);
        return true;
    }
    const uint64_t lo    = tick + 1u > FirstFrameTick() ? tick + 1u : FirstFrameTick();
    const uint64_t start = (lo + frame_ - 1u) / frame_ * frame_;
    if (start >= end_) return false;
    at = start;
    return true;
}

void Pr31x00LcdDfSignal::Save(StateWriter& w, uint64_t tick) {
    w.Write("df_toggles", TogglesThrough(tick));
    w.Write("df_tick", tick);
    w.Write("df_frame", frame_);
    w.Write("df_end", end_);
    w.Write<uint8_t>("df_every_frame", every_frame_ ? 1u : 0u);
    counter_.Save(w, "df_", tick);
}

void Pr31x00LcdDfSignal::Restore(StateReader& r) {
    uint8_t every_frame = 0;
    r.Read("df_toggles", done_);
    r.Read("df_tick", epoch_tick_);
    r.Read("df_frame", frame_);
    r.Read("df_end", end_);
    r.Read("df_every_frame", every_frame);
    counter_.Restore(r, "df_");
    counter_.Rebase(epoch_tick_);
    every_frame_ = every_frame != 0u;
    epoch_flags_ = counter_.FlagTotalThrough(epoch_tick_);
    from_start_  = false;
}

}

#include "vr41xx_clock_state.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

void Vr41xxClockState::OnReady() {
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
        if (kind == ResetLineKind::Rtc) {
            pending_ = 0;
            active_  = 0;
        } else {
            active_ = pending_;
        }
    });
}

const char* Vr41xxClockState::WritePending(uint16_t value) {
    if (const char* why = RejectedPending(value)) return why;
    pending_ = value;
    return nullptr;
}

void Vr41xxClockState::SaveState(StateWriter& w) const {
    w.Write("div_pending", pending_);
    w.Write("div_active", active_);
}

void Vr41xxClockState::RestoreState(StateReader& r) {
    r.Read("div_pending", pending_);
    r.Read("div_active", active_);
    ActiveRestored();
}

}

#include "vr41xx_unit_clock_gate.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include <cerfengine/mips/mips_interrupt_channel.h>
#include "../../execution/engine_bridge.h"
#include "vr41xx_cmu.h"

#include <utility>

namespace cerfcore {

void Vr41xxUnitClockGate::Attach(uint16_t cmu_mask, std::function<void()> on_change) {
    cmu_       = &emu_.Get<Vr41xxCmu>();
    channel_   = &emu_.Get<EngineBridge>().Engine().Get<cerfengine::MipsInterruptChannel>();
    mask_      = cmu_mask;
    on_change_ = std::move(on_change);
    cmu_->RegisterClockListener(mask_, [this](bool supplied_after) {
        supplied_ = supplied_after;
        on_change_();
    });
    channel_->RegisterSuspendListener([this] {
        suspended_ = channel_->Suspended();
        on_change_();
    });
    emu_.Get<GuestCpuReset>().RegisterResetReleaseListener([this] { Refresh(); });
}

void Vr41xxUnitClockGate::Refresh() {
    supplied_  = cmu_->SuppliedClocks(mask_) != 0u;
    suspended_ = channel_->Suspended();
    on_change_();
}

void Vr41xxUnitClockGate::Save(StateWriter& w) const {
    w.Write<uint8_t>("clock_supplied", supplied_ ? 1u : 0u);
    w.Write<uint8_t>("suspended", suspended_ ? 1u : 0u);
}

void Vr41xxUnitClockGate::Restore(StateReader& r) {
    uint8_t supplied = 0u, suspended = 0u;
    r.Read("clock_supplied", supplied);
    r.Read("suspended", suspended);
    supplied_  = supplied != 0u;
    suspended_ = suspended != 0u;
}

}

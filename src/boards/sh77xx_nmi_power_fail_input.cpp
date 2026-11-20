#include "sh77xx_nmi_power_fail_input.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/state/emulation_freeze.h>
#include <cerfcore/state/state_stream.h>
#include "../socs/sh77xx/sh77xx_intc.h"

#include <cstdint>

namespace cerfcore {

void Sh77xxNmiPowerFailInput::OnReady() {
    intc_ = &emu_.Get<Sh77xxIntc>();
    intc_->SetNmiLevel(true);
    emu_.Get<MachineObjectTree>().Add(this);
}

void Sh77xxNmiPowerFailInput::SetPowerFailed(bool failed) {
    {
        auto frozen = emu_.Get<EmulationFreeze>().WorkerSection();
        failed_.store(failed, std::memory_order_release);
        intc_->SetNmiLevel(!failed);
    }
    Changed();
}

void Sh77xxNmiPowerFailInput::SaveObjectState(StateWriter& w) const {
    w.Write<uint8_t>("power_failed", PowerFailed() ? 1u : 0u);
}

void Sh77xxNmiPowerFailInput::RestoreObjectState(StateReader& r) {
    uint8_t failed = 0u;
    r.Read("power_failed", failed);
    failed_.store(failed != 0u, std::memory_order_release);
    Changed();
}

}

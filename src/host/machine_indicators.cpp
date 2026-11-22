#include <cerfcore/host/machine_indicators.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/host_input_activity.h>
#include <cerfcore/host/vibration_outputs.h>

namespace cerfcore {

void MachineButton::Down() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        ForgetHoldsAfterRestoreLocked();
        if (holders_ != 0) {
            ++holders_;
            return;
        }
        if (!Pressed()) return;
        holders_       = 1;
        seen_restores_ = restores_.load(std::memory_order_acquire);
    }
    emu_.Get<HostInputActivity>().Delivered();
}

void MachineButton::Up() {
    std::lock_guard<std::mutex> lk(mtx_);
    ForgetHoldsAfterRestoreLocked();
    if (holders_ == 0 || --holders_ != 0) return;
    Released();
}

void MachineButton::RestoreObjectState(StateReader&) { restores_.fetch_add(1, std::memory_order_release); }

void MachineButton::ForgetHoldsAfterRestoreLocked() {
    const uint64_t restores = restores_.load(std::memory_order_acquire);
    if (restores == seen_restores_) return;
    seen_restores_ = restores;
    holders_       = 0;
}

void MachineLamp::Refresh() {
    if (published_.Replace([this] { return Lamp(); })) Changed();
}

void MachineVibrator::Refresh() {
    published_.Replace([this] { return Vibrating(); }, [this](bool vibrating) {
        Changed();
        if (VibrationOutputs* outputs = emu_.TryGet<VibrationOutputs>()) outputs->SetVibrating(*this, vibrating);
    });
}

void MachineCharacterDisplay::Refresh() {
    if (published_.Replace([this] { return Characters(); })) Changed();
}

}

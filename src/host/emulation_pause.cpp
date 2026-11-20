#include <cerfcore/host/emulation_pause.h>

#include <cerfcore/core/cerf_emulator.h>
#include "../core/steady_time.h"
#include <cerfcore/execution/jit_runner.h>
#include <cerfcore/host/notification_stack.h>

namespace cerfcore {

REGISTER_SERVICE(EmulationPause);

namespace {

constexpr char kPausedText[] = "The emulator is paused";

}

bool EmulationPause::UserCanToggle() const {
    return !state_operation_.load(std::memory_order_acquire) &&
           emu_.Get<JitRunner>().Started();
}

void EmulationPause::Toggle() { SetUserPaused(!IsUserPaused()); }

bool EmulationPause::SetUserPaused(bool paused) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!UserCanToggle()) return false;
        if (paused != user_.load(std::memory_order_acquire)) {
            changed = Apply(paused, false);
            ShowUserCard(paused);
        }
    }
    if (changed) change_listeners_.Call();
    return true;
}

void EmulationPause::BeginStateOperation() {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto& runner = emu_.Get<JitRunner>();
        runner.HoldResets(true);
        changed = Apply(user_.load(std::memory_order_acquire), true);
        runner.Pause();
    }
    if (changed) change_listeners_.Call();
}

void EmulationPause::EndStateOperation(bool release_user_pause) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const bool was_user = user_.load(std::memory_order_acquire);
        const bool user     = was_user && !release_user_pause;
        changed             = Apply(user, false);
        emu_.Get<JitRunner>().HoldResets(false);
        if (was_user && !user) ShowUserCard(false);
    }
    if (changed) change_listeners_.Call();
}

void EmulationPause::RegisterChangeListener(std::function<void()> fn) { change_listeners_.Add(std::move(fn)); }

bool EmulationPause::Apply(bool user, bool state_operation) {
    const bool was      = IsPaused();
    const bool was_user = user_.load(std::memory_order_acquire);
    const bool now      = user || state_operation;
    if (now && !was) pause_tick_ms_.store(HostSteadyMicros() / 1000, std::memory_order_release);
    user_.store(user, std::memory_order_release);
    state_operation_.store(state_operation, std::memory_order_release);
    if (now != was) {
        auto& runner = emu_.Get<JitRunner>();
        if (now) runner.Pause();
        else     runner.Resume();
    }
    return now != was || user != was_user;
}

void EmulationPause::ShowUserCard(bool shown) {
    auto& stack = emu_.Get<NotificationStack>();
    if (shown) {
        stack.Show(NotificationId::EmulatorPaused, NotificationKind::Warning, kPausedText);
    } else {
        stack.Close(NotificationId::EmulatorPaused);
    }
}

uint64_t EmulationPause::AnimationTickMs() const {
    return IsPaused() ? pause_tick_ms_.load(std::memory_order_acquire)
                      : HostSteadyMicros() / 1000;
}

}

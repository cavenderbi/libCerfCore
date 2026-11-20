#pragma once

#include "../core/service.h"
#include "guest_power_notifier.h"
#include "../core/callback_list.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace cerfcore {

class BootSleep;
class GuestCpuReset;
class StateReader;
class StateWriter;

/* Implemented by the SoC peripheral owning the sleep-wake reset cause (SA-1110
   RCSR.SMR, PXA255 RCSR.SMR). Non-Service so a Peripheral can implement it; the
   implementer self-registers with GuestDeepSleep from its OnReady. */
class DeepSleepWaker {
public:
    virtual ~DeepSleepWaker() = default;
    /* Latch the sleep-mode reset cause so the OAL boot path resumes on wake. */
    virtual void LatchSleepWakeCause() = 0;
    /* Clear it on a non-resume reset. CERF's resume jumps straight to the saved
       vector, skipping the OAL/EBOOT code that clears this cause on real HW, so a
       later reboot would re-read the stale cause and resume from a now-invalid
       save block. Cleared on every SetResetPending(is_resume=false). */
    virtual void ClearSleepWakeCause() = 0;
};

/* A board whose boot ROM resumes the guest at a saved vector on a sleep wake
   (simpad: the PSPR vector its kernel `start` stores; DevEmu: the SLEEPDATA
   WakeAddr EBOOT jumps to). Self-registers with GuestDeepSleep from OnReady;
   absent → the guest resumes at the cold entry. */
class SleepResumeVectorProvider {
public:
    virtual ~SleepResumeVectorProvider() = default;
    /* Arm the next reset delivery with the board's resume state, on its own CPU. */
    virtual void ApplyPendingResume() = 0;
};

/* TMPR3911 §12.2.4 (p.12-8): "the CPU will remain suspended at its last state ...
   Then the CPU will resume the instruction execution from where it stopped."
   OnPowerUp applies what the silicon asserts on the power-up edge. */
class DeepSleepClockStop {
public:
    virtual ~DeepSleepClockStop() = default;
    virtual void OnPowerUp() = 0;
};

class GuestDeepSleep : public Service {
public:
    using Service::Service;

    void OnReady() override;

    /* OnReady-time only; at most one waker per emulator instance. */
    void RegisterWaker(DeepSleepWaker* waker);

    /* OnReady-time only; at most one per emulator instance, and never together
       with a DeepSleepWaker. */
    void RegisterClockStopWaker(DeepSleepClockStop* waker);

    void RegisterPowerOnWake();

    /* OnReady-time only. Listeners model silicon on a power rail the Suspend State
       cuts (TMPR3911 §12.2.3, p.12-7: "VSTANDBY and VCCDRAM are powered but VCC3 is
       not powered"): they lose power with the CPU clock and re-initialise on the
       power-up edge, which no reset line reaches. */
    void RegisterPowerUpListener(std::function<void()> fn);

    /* Any thread. Clears the latched sleep-wake cause; called from a non-resume
       reset so the rebooting guest doesn't misread it as another sleep wake. */
    void ClearWakeCause();

    /* OnReady-time, board-scoped: supplies the sleep-wake resume vector. */
    void RegisterResumeVectorProvider(SleepResumeVectorProvider* p);

    static constexpr int64_t kNoParkWake = INT64_MAX;

    void RegisterUserWakeInput(std::function<void()> fn);
    void RegisterSleepEntryListener(std::function<void()> fn);
    void RegisterParkClock(std::function<void()> fn);
    void RegisterParkWakeSource(std::function<bool()> fn);
    void RegisterParkWakeDue(std::function<int64_t()> fn);
    void RefreshParkWakeDue();
    void PollPark();
    void FinishPark();

    int64_t SleptNs() const;
    int64_t ParkWaitNs() const;

    void Enter();
    void Resume();

    void RegisterSleepListener(std::function<void(bool asleep)> fn);
    void ObserveAsleep(bool asleep);
    bool Asleep() const { return asleep_.load(std::memory_order_acquire); }

    void RegisterSuspendListener(std::function<void(bool suspended)> fn);
    bool Suspended() const { return suspended_.load(std::memory_order_acquire); }

    void OnFullRestore();
    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    void OnGuestIdle();
    bool ClaimSleepEnd();
    void RequireNoWakeShape(const char* shape) const;
    void ConsumeSleepSpan();
    int64_t CollectParkWakeDue() const;
    void DeliverWake(ResumeSource src);
    void PowerUpClockStop();

    CallbackList<>                     power_up_listeners_;
    CallbackList<>                     user_wake_inputs_;
    CallbackList<>                     sleep_entry_listeners_;
    CallbackList<bool>                 sleep_listeners_;
    CallbackList<bool>                 suspend_listeners_;
    CallbackList<>                     park_clocks_;
    std::vector<std::function<bool()>> park_wake_sources_;
    std::vector<std::function<int64_t()>> park_wake_dues_;
    mutable std::mutex         span_mtx_;
    int64_t                    slept_ns_       = 0;
    int64_t                    sleep_entry_ns_ = 0;
    int64_t                    park_due_ns_    = kNoParkWake;
    std::atomic<bool>          wake_claimed_{false};
    DeepSleepClockStop*        clock_stop_             = nullptr;
    DeepSleepWaker*            waker_                  = nullptr;
    SleepResumeVectorProvider* resume_vector_provider_ = nullptr;
    BootSleep*                 boot_sleep_             = nullptr;
    GuestCpuReset*             reset_                  = nullptr;
    bool                       power_on_wake_          = false;
    bool                       idled_since_boot_       = false;
    bool                       in_boot_sleep_          = false;
    std::atomic<bool>          asleep_{false};
    std::atomic<bool>          suspended_{false};
    std::atomic<bool>          off_{false};
    bool                       wake_requested_ = false;
    std::mutex                 resume_mtx_;
};

}

#pragma once

#include "../core/callback_list.h"
#include "../core/service.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>

namespace cerfcore {

class EmulationPause : public Service {
public:
    using Service::Service;

    void Toggle();
    bool SetUserPaused(bool paused);
    bool IsPaused() const {
        return user_.load(std::memory_order_acquire) ||
               state_operation_.load(std::memory_order_acquire);
    }
    bool IsUserPaused() const { return user_.load(std::memory_order_acquire); }
    bool UserCanToggle() const;

    void BeginStateOperation();
    void EndStateOperation(bool release_user_pause);

    void RegisterChangeListener(std::function<void()> fn);

    uint64_t AnimationTickMs() const;

private:
    bool Apply(bool user, bool state_operation);
    void ShowUserCard(bool shown);

    std::mutex            mtx_;
    std::atomic<bool>     user_{false};
    std::atomic<bool>     state_operation_{false};
    std::atomic<uint64_t> pause_tick_ms_{0};
    CallbackList<>        change_listeners_;
};

}

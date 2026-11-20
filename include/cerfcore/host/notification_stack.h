#pragma once

#include "../core/callback_list.h"
#include "../core/service.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cerfcore {

enum class NotificationKind : uint8_t { Warning, Error, Success };

enum class NotificationId : uint32_t {
    EmulatorPaused   = 1,
    SavingState      = 2,
    LoadingState     = 3,
    GuestSleeping    = 4,
    SavingCrashState = 5,
};

struct NotificationCard {
    uint32_t         id;
    NotificationKind kind;
    std::string      text;
};

class NotificationStack : public Service {
public:
    using Service::Service;

    void OnShutdown() override;

    void Show(NotificationId id, NotificationKind kind, std::string text);
    void Close(NotificationId id);
    void Post(NotificationKind kind, std::string text, std::chrono::milliseconds timeout = {});
    void PostError(std::string text);
    void Dismiss(uint32_t id);

    void RegisterChangeListener(std::function<void()> fn);

    uint64_t Generation() const { return gen_.load(std::memory_order_acquire); }
    uint64_t Snapshot(std::vector<NotificationCard>& out) const;

private:
    using Clock = std::chrono::steady_clock;

    struct Expiry {
        uint32_t          id;
        Clock::time_point at;
    };

    bool Remove(uint32_t id, const char* how);
    void NotifyChanged();
    void ExpireLoop();

    static constexpr uint32_t kFirstPostId = 0x100u;

    mutable std::mutex            mtx_;
    std::condition_variable       wake_;
    std::vector<NotificationCard> cards_;
    std::vector<Expiry>           expiries_;
    uint32_t                      next_post_id_ = kFirstPostId;
    bool                          stop_         = false;
    std::thread                   expiry_worker_;
    std::atomic<uint64_t>         gen_{0};
    CallbackList<>                change_listeners_;
};

}

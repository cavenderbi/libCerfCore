#pragma once

#include "../core/callback_list.h"
#include "../core/service.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <shared_mutex>

namespace cerfcore {

class HostPointerInputLock : public Service {
public:
    using Service::Service;

    void OnReady() override;

    class Hold {
    public:
        Hold(Hold&& other) noexcept;
        Hold(const Hold&)            = delete;
        Hold& operator=(const Hold&) = delete;
        Hold& operator=(Hold&&)      = delete;
        ~Hold();

    private:
        friend class HostPointerInputLock;
        Hold(HostPointerInputLock& lock, uint64_t owner) : lock_(&lock), owner_(owner) {}

        HostPointerInputLock* lock_;
        uint64_t              owner_;
    };

    Hold Acquire();
    bool Held() const;
    bool HeldBy(const Hold& hold) const;
    bool Admits(const Hold& hold) const;

    template <class Forward>
    bool AdmitUnlessHeld(Forward&& forward) {
        std::shared_lock<std::shared_mutex> shared(gate_);
        if (Held()) return false;
        forward();
        return true;
    }

    template <class Forward>
    void RunUnlessHeld(Forward&& forward) {
        if (AdmitUnlessHeld(forward)) Admitted();
    }

    template <class Forward>
    void RunIfAdmitted(const Hold& hold, Forward&& forward) {
        std::shared_lock<std::shared_mutex> shared(gate_);
        if (Admits(hold)) forward();
    }

    void RegisterAcquireListener(std::function<void()> fn);

private:
    void Release(uint64_t owner);
    void Admitted();

    mutable std::shared_mutex gate_;
    std::atomic<uint64_t>     owner_{0};
    std::atomic<uint64_t> next_owner_{1};
    CallbackList<>        acquire_listeners_;
};

}

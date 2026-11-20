#include <cerfcore/host/notification_stack.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/core/log.h>

#include <algorithm>

namespace cerfcore {

REGISTER_SERVICE(NotificationStack);

void NotificationStack::OnShutdown() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    wake_.notify_all();
    if (expiry_worker_.joinable()) expiry_worker_.join();
}

void NotificationStack::RegisterChangeListener(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(mtx_);
    change_listeners_.Add(std::move(fn));
}

void NotificationStack::NotifyChanged() {
    CallbackList<> listeners;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        listeners = change_listeners_;
    }
    listeners.Call();
}

void NotificationStack::Show(NotificationId id, NotificationKind kind, std::string text) {
    const uint32_t key = static_cast<uint32_t>(id);
    LOG(Cerf, "[NOTIFY] show id=%u '%s'\n", key, text.c_str());
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = std::find_if(cards_.begin(), cards_.end(),
                               [key](const NotificationCard& c) { return c.id == key; });
        if (it != cards_.end()) {
            it->kind = kind;
            it->text = std::move(text);
        } else {
            cards_.push_back({key, kind, std::move(text)});
        }
        gen_.fetch_add(1, std::memory_order_acq_rel);
    }
    NotifyChanged();
}

void NotificationStack::Post(NotificationKind kind, std::string text, std::chrono::milliseconds timeout) {
    const bool       timed   = timeout.count() > 0;
    InstanceThreads* threads = nullptr;
    if (timed) threads = &emu_.Get<InstanceThreads>();
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const uint32_t key = next_post_id_++;
        LOG(Cerf, "[NOTIFY] post id=%u kind=%u timeout=%lld ms '%s'\n", key, static_cast<unsigned>(kind),
            static_cast<long long>(timeout.count()), text.c_str());
        cards_.push_back({key, kind, std::move(text)});
        gen_.fetch_add(1, std::memory_order_acq_rel);
        if (timed && !stop_) {
            expiries_.push_back({key, Clock::now() + timeout});
            if (!expiry_worker_.joinable()) expiry_worker_ = threads->Start([this] { ExpireLoop(); });
            wake_.notify_all();
        }
    }
    NotifyChanged();
}

void NotificationStack::PostError(std::string text) { Post(NotificationKind::Error, std::move(text)); }

void NotificationStack::Close(NotificationId id) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        removed = Remove(static_cast<uint32_t>(id), "closed");
    }
    if (removed) NotifyChanged();
}

void NotificationStack::Dismiss(uint32_t id) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        removed = Remove(id, "dismissed");
    }
    if (removed) NotifyChanged();
}

bool NotificationStack::Remove(uint32_t id, const char* how) {
    expiries_.erase(std::remove_if(expiries_.begin(), expiries_.end(), [id](const Expiry& e) { return e.id == id; }),
                    expiries_.end());
    auto it = std::find_if(cards_.begin(), cards_.end(),
                           [id](const NotificationCard& c) { return c.id == id; });
    if (it == cards_.end()) return false;
    cards_.erase(it);
    LOG(Cerf, "[NOTIFY] %s id=%u\n", how, id);
    gen_.fetch_add(1, std::memory_order_acq_rel);
    return true;
}

void NotificationStack::ExpireLoop() {
    std::unique_lock<std::mutex> lk(mtx_);
    while (!stop_) {
        const Clock::time_point now     = Clock::now();
        bool                    removed = false;
        std::vector<uint32_t>   due;
        for (const Expiry& e : expiries_)
            if (e.at <= now) due.push_back(e.id);
        for (uint32_t id : due) removed = Remove(id, "expired") || removed;
        if (removed) {
            lk.unlock();
            NotifyChanged();
            lk.lock();
            continue;
        }
        if (expiries_.empty()) {
            wake_.wait(lk);
            continue;
        }
        const auto earliest = std::min_element(expiries_.begin(), expiries_.end(),
                                               [](const Expiry& a, const Expiry& b) { return a.at < b.at; });
        wake_.wait_until(lk, earliest->at);
    }
}

uint64_t NotificationStack::Snapshot(std::vector<NotificationCard>& out) const {
    std::lock_guard<std::mutex> lk(mtx_);
    out = cards_;
    return gen_.load(std::memory_order_acquire);
}

}

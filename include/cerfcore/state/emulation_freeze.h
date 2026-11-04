#pragma once

#include "../core/service.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace cerfcore {

/* Freezes peripheral worker threads for a consistent hibernation snapshot.
   Lock-order invariant (deadlocks if violated): the freeze lock is taken BEFORE
   any peripheral mutex, and a worker never holds WorkerSection across a
   wait/sleep. */
class EmulationFreeze : public Service {
public:
    using Service::Service;

    /* Worker threads: hold the returned guard around the part of an iteration
       that reads or writes guest-visible state. Never hold it across a
       cv wait / sleep / thread join. */
    [[nodiscard]] std::shared_lock<std::shared_mutex> WorkerSection() {
        return std::shared_lock<std::shared_mutex>(mtx_);
    }

    /* ONLY for a worker the snapshot holder itself joins (Restore tears down endpoints
       and cards); it would park on the lock the joiner owns. Check owns_lock(). */
    [[nodiscard]] std::shared_lock<std::shared_mutex> TryWorkerSection() {
        return std::shared_lock<std::shared_mutex>(mtx_, std::try_to_lock);
    }

    /* Hibernation: hold the returned guard across the whole save or restore. */
    [[nodiscard]] std::unique_lock<std::shared_mutex> SnapshotSection() {
        return std::unique_lock<std::shared_mutex>(mtx_);
    }

    void RunOrQueue(std::function<void()> fn);
    void HoldQueue();
    void ReleaseQueue();

private:
    void RunAsOwner(const std::function<void()>& fn);

    std::shared_mutex                  mtx_;
    std::mutex                         queue_mtx_;
    std::vector<std::function<void()>> queue_;
    bool                               holding_ = false;
    std::atomic<std::thread::id>       running_thread_{};
};

}

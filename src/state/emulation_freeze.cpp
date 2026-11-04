#include <cerfcore/state/emulation_freeze.h>

#include <cerfcore/core/cerf_emulator.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(EmulationFreeze);

void EmulationFreeze::RunOrQueue(std::function<void()> fn) {
    if (running_thread_.load(std::memory_order_acquire) == std::this_thread::get_id()) {
        fn();
        return;
    }
    std::lock_guard<std::mutex> lk(queue_mtx_);
    if (holding_) {
        queue_.push_back(std::move(fn));
        return;
    }
    auto section = WorkerSection();
    RunAsOwner(fn);
}

void EmulationFreeze::HoldQueue() {
    std::lock_guard<std::mutex> lk(queue_mtx_);
    holding_ = true;
}

void EmulationFreeze::ReleaseQueue() {
    std::lock_guard<std::mutex> lk(queue_mtx_);
    holding_ = false;
    std::vector<std::function<void()>> ready;
    ready.swap(queue_);
    auto section = WorkerSection();
    for (auto& fn : ready) RunAsOwner(fn);
}

void EmulationFreeze::RunAsOwner(const std::function<void()>& fn) {
    struct Owner {
        std::atomic<std::thread::id>& slot;
        explicit Owner(std::atomic<std::thread::id>& s) : slot(s) {
            slot.store(std::this_thread::get_id(), std::memory_order_release);
        }
        ~Owner() { slot.store(std::thread::id{}, std::memory_order_release); }
    } owner(running_thread_);
    fn();
}

}

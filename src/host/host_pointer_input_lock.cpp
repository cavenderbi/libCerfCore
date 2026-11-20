#include <cerfcore/host/host_pointer_input_lock.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/host_input_activity.h>
#include <cerfcore/host/pointer_input.h>
#include <cerfcore/host/relative_mouse_input.h>
#include <cerfcore/host/touch_input.h>
#include <cerfcore/state/hibernation.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(HostPointerInputLock);

void HostPointerInputLock::OnReady() {
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { owner_.store(0, std::memory_order_release); });
}

HostPointerInputLock::Hold::Hold(Hold&& other) noexcept
    : lock_(std::exchange(other.lock_, nullptr)), owner_(other.owner_) {}

HostPointerInputLock::Hold::~Hold() {
    if (lock_) lock_->Release(owner_);
}

HostPointerInputLock::Hold HostPointerInputLock::Acquire() {
    const uint64_t owner = next_owner_.fetch_add(1, std::memory_order_relaxed);
    Hold           hold(*this, owner);
    {
        std::unique_lock<std::shared_mutex> exclusive(gate_);
        owner_.store(owner, std::memory_order_release);
        if (auto* touch = emu_.TryGet<TouchInput>()) touch->ReleaseHeld(hold);
        if (auto* pointer = emu_.TryGet<PointerInput>()) pointer->ReleaseHeld(hold);
        if (auto* relative = emu_.TryGet<RelativeMouseInput>()) relative->ReleaseHeld(hold);
    }
    acquire_listeners_.Call();
    return hold;
}

bool HostPointerInputLock::Held() const { return owner_.load(std::memory_order_acquire) != 0; }

bool HostPointerInputLock::HeldBy(const Hold& hold) const {
    return owner_.load(std::memory_order_acquire) == hold.owner_;
}

bool HostPointerInputLock::Admits(const Hold& hold) const { return !Held() || HeldBy(hold); }

void HostPointerInputLock::RegisterAcquireListener(std::function<void()> fn) {
    acquire_listeners_.Add(std::move(fn));
}

void HostPointerInputLock::Release(uint64_t owner) {
    uint64_t expected = owner;
    owner_.compare_exchange_strong(expected, 0, std::memory_order_acq_rel);
}

void HostPointerInputLock::Admitted() { emu_.Get<HostInputActivity>().Delivered(); }

}

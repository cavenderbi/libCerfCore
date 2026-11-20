#include <cerfcore/host/keyboard_router.h>

#include "../core/deferred_jobs.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/host_input_activity.h>
#include <cerfcore/host/keyboard_input.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/host/virtual_keys.h>

namespace cerfcore {

REGISTER_SERVICE(KeyboardRouter);

void KeyboardRouter::OnReady() { emu_.Get<MachineObjectTree>().Add(this); }

void KeyboardRouter::SaveObjectState(StateWriter& w) const { SaveSelection(w); }

void KeyboardRouter::RestoreObjectState(StateReader& r) { RestoreSelection(r); }

void KeyboardRouter::OnHostKey(uint8_t vk, bool key_up) { Route(vk, key_up, false); }

void KeyboardRouter::OnStockKey(uint8_t vk, bool key_up) { Route(vk, key_up, true); }

void KeyboardRouter::SendCtrlAltDel() {
    OnHostKey(kVkControl, false);
    OnHostKey(kVkMenu, false);
    OnHostKey(kVkDelete, false);
    OnHostKey(kVkDelete, true);
    OnHostKey(kVkMenu, true);
    OnHostKey(kVkControl, true);
}

KeyboardInput* KeyboardRouter::Stock() const {
    for (KeyboardInput* s : Sources())
        if (s->Stock()) return s;
    return nullptr;
}

void KeyboardRouter::Route(uint8_t vk, bool key_up, bool stock) {
    KeyboardInput* receiver = nullptr;
    if (!key_up) receiver = stock ? Stock() : Active();
    KeyboardInput* target;
    KeyboardInput* released = nullptr;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        KeyboardInput*& owner = key_owner_[vk];
        if (key_up) {
            target = owner;
            owner  = nullptr;
        } else {
            if (owner != receiver) released = owner;
            owner  = receiver;
            target = owner;
        }
    }
    auto& jobs = emu_.Get<DeferredJobs>();
    if (released) jobs.PostToGuestThread([released, vk] { released->OnHostKey(vk, true); });
    if (!target) return;
    jobs.PostToGuestThread([target, vk, key_up] { target->OnHostKey(vk, key_up); });
    emu_.Get<HostInputActivity>().Delivered();
}

bool KeyboardRouter::Held(uint8_t vk) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return key_owner_[vk] != nullptr;
}

}

#include <cerfcore/host/host_input_activity.h>

#include <cerfcore/core/cerf_emulator.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(HostInputActivity);

void HostInputActivity::RegisterListener(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(mtx_);
    listeners_.Add(std::move(fn));
}

void HostInputActivity::Delivered() {
    std::lock_guard<std::mutex> lk(mtx_);
    listeners_.Call();
}

}

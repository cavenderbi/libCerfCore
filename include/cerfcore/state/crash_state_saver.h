#pragma once

#include "../core/callback_list.h"
#include "../core/service.h"

#include <atomic>
#include <functional>
#include <string>

namespace cerfcore {

class CrashStateSaver : public Service {
public:
    using Service::Service;

    void OnReady() override;

    void        SetEnabled(bool enabled) { enabled_.store(enabled, std::memory_order_release); }
    std::string Path() const;

    void RegisterSavedListener(std::function<void(const std::string& path)> fn);

private:
    void SaveIfHalted();

    std::atomic<bool>                enabled_{false};
    CallbackList<const std::string&> saved_listeners_;
};

}

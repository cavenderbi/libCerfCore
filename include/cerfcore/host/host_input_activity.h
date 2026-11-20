#pragma once

#include "../core/callback_list.h"
#include "../core/service.h"

#include <functional>
#include <mutex>

namespace cerfcore {

class HostInputActivity : public Service {
public:
    using Service::Service;

    void RegisterListener(std::function<void()> fn);
    void Delivered();

private:
    std::mutex     mtx_;
    CallbackList<> listeners_;
};

}

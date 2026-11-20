#pragma once

#include "../core/callback_list.h"
#include "../core/service.h"

#include <functional>
#include <string_view>

namespace cerfcore {

class KernelDebugSink;

class GuestDebugLines : public Service {
public:
    using Service::Service;
    using Listener = std::function<void(std::string_view line, std::string_view source)>;

    void Subscribe(Listener listener);

private:
    friend class KernelDebugSink;

    void Publish(std::string_view line, std::string_view source);

    CallbackList<std::string_view, std::string_view> listeners_;
};

}

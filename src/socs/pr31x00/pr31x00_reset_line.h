#pragma once

#include <cerfcore/core/service.h>

#include <functional>
#include <vector>

namespace cerfcore {

class GuestCpuReset;

class Pr31x00ResetLine : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void RegisterResetListener(std::function<void()> fn);
    void RegisterReleaseListener(std::function<void()> fn);

    bool Resetting() const;

private:
    GuestCpuReset*                     reset_ = nullptr;
    std::vector<std::function<void()>> reset_listeners_;
    std::vector<std::function<void()>> release_listeners_;
};

}

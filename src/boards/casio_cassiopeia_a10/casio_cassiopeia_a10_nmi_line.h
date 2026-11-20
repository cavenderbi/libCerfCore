#pragma once

#include <cerfcore/core/service.h>

#include <mutex>

namespace cerfcore {

class CasioCassiopeiaA10Register00E;
class Sh77xxIntc;

class CasioCassiopeiaA10NmiLine : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void SetWakeRequest(bool asserted);
    void SetOnKey(bool down);
    bool OnKeyDown();

private:
    void DriveLocked();

    std::mutex                     mtx_;
    Sh77xxIntc*                    intc_         = nullptr;
    CasioCassiopeiaA10Register00E* register_00e_ = nullptr;
    bool        wake_request_ = false;
    bool        on_key_down_  = false;
};

}

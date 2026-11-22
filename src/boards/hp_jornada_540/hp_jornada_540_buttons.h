#pragma once

#include <cerfcore/core/service.h>

#include <atomic>
#include <cstdint>

namespace cerfcore {

class HpJornada540KeyWake;
class Sh77xxIntc;

enum class HpJornada540Button : uint8_t {
    kUp,
    kDown,
    kAction,
    kRecord,
    kNotification,
    kQuickLaunch1,
    kQuickLaunch2,
    kQuickLaunch3,
    kQuickLaunch4,
    kOnOff,
    kCount,
};

class HpJornada540Buttons : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void SetPressed(HpJornada540Button button, bool pressed);
    bool Pressed(HpJornada540Button button) const;

private:
    void Publish(HpJornada540Button button);
    void PublishQuickLaunch();
    void ReleaseAll();

    Sh77xxIntc*           intc_     = nullptr;
    HpJornada540KeyWake*  key_wake_ = nullptr;
    std::atomic<uint32_t> pressed_ = 0u;
};

}

#pragma once

#include <cerfcore/core/service.h>
#include "../../execution/guest_cycle_clock.h"

#include <atomic>
#include <cstdint>

namespace cerfcore {

class CasioCassiopeiaA10NmiLine;
class Hd6417096OnKeyInput;

class CasioCassiopeiaA10OnKey : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void Press();
    void SetHeld(bool down);

private:
    void StartButtonPress();
    void EndButtonPress();
    void ArmRelease();
    void Hold(bool cpu_running);
    void Release();

    Hd6417096OnKeyInput*       on_key_input_  = nullptr;
    CasioCassiopeiaA10NmiLine* nmi_line_      = nullptr;
    GuestCycleClock*           clock_         = nullptr;
    GuestCycleClock::Event*    release_       = nullptr;
    std::atomic<bool>          standby_{false};
    bool                       held_          = false;
    bool                       button_press_  = false;
    int64_t                    release_at_ns_ = 0;
};

}

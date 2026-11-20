#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/state/state_stream.h>
#include "../../execution/guest_cycle_clock.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

class GuestCpuReset;
class Sh77xxCpg;
class Sh77xxFrqcr;

class Sh77xxNmiInput : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void SetLevel(bool high);
    bool High() const;
    bool Nmie() const;
    void WriteNmie(bool nmie);
    bool Pending() const;
    void Accept();
    void Reset();
    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);
    void PostRestore();

private:
    enum class Window : uint8_t { kNone, kUndetected, kAmbiguous };

    void ArmWindowLocked();
    void EndWindowStep();

    mutable std::mutex      mtx_;
    GuestCycleClock*        clock_          = nullptr;
    GuestCycleClock::Event* window_event_   = nullptr;
    GuestCpuReset*          reset_          = nullptr;
    Sh77xxCpg*              cpg_            = nullptr;
    Sh77xxFrqcr*            frqcr_          = nullptr;
    uint64_t                undetected_end_ = 0u;
    uint64_t                ambiguous_end_  = 0u;
    Window                  window_         = Window::kNone;
    bool                    level_known_    = false;
    bool                    high_           = false;
    bool                    nmie_           = false;
    bool                    pending_        = false;
};

}

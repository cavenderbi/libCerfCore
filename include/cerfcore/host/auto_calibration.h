#pragma once

#include "../core/board_database.h"
#include "../core/callback_list.h"
#include "../core/service.h"
#include "calibration_cross_scan.h"
#include "calibration_host.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace cerfcore {

struct DeviceConfig;
class CalibrationFrameSource;
class CalibrationRun;
class HostPresentation;
class InstanceThreads;

enum class CalibrationTrigger : uint8_t { kBootWindow, kRequest, kGuestAdditions };

enum class CalibrationResult : uint8_t { kFinished, kFailed, kDeclined, kAborted, kResolutionSwitched };

struct CalibrationRequest {
    CalibrationTrigger  trigger          = CalibrationTrigger::kRequest;
    std::optional<bool> calibrate        = std::nullopt;
    std::optional<bool> panel_resolution = std::nullopt;
    bool                host_notices     = true;
};

class AutoCalibration : public Service {
public:
    using Service::Service;
    using ResultListener = std::function<void(uint64_t cycle, CalibrationTrigger, CalibrationResult)>;

    void OnReady() override;
    void OnShutdown() override;

    bool                    CanPress() const;
    std::optional<uint64_t> Request(const CalibrationRequest& request);
    void                    RegisterResultListener(ResultListener listener);

private:
    using Clock = std::chrono::steady_clock;

    struct Cycle {
        uint64_t           id = 0;
        CalibrationRequest request;
    };

    void  BeginWindow();
    void  StopWindow();
    void  SetBusy(bool busy);
    Cycle ClaimBootFind();
    void  EnsureWorkerLocked();
    void  Loop();
    void  Tick();
    void  Explicit(const Cycle& cycle);
    void  Attempt(const Cycle& cycle, const CalibrationFrame& frame, const CalibrationCross& cross,
                  uint64_t generation);
    bool  ResolutionAllows(const Cycle& cycle);
    bool  SwitchToPanelResolution(uint32_t width, uint32_t height);
    CalibrationAnswer CalibrationApproved(const Cycle& cycle);
    bool              PanelResolutionApproved(const Cycle& cycle, uint32_t width, uint32_t height);
    void              Fail(const Cycle& cycle, bool host_notices, const char* reason);
    void  Report(const Cycle& cycle, CalibrationResult result);

    static const char* TriggerName(CalibrationTrigger trigger);

    CalibrationHost*              host_          = nullptr;
    CalibrationFrameSource*       frames_        = nullptr;
    HostPresentation*             presentation_  = nullptr;
    DeviceConfig*                 config_        = nullptr;
    InstanceThreads*              threads_       = nullptr;
    CalibrationRun*               run_           = nullptr;
    std::optional<DbLcdPanelSize> panel_;

    CallbackList<uint64_t, CalibrationTrigger, CalibrationResult> result_listeners_;

    std::mutex              mutex_;
    std::condition_variable wake_;
    std::thread             worker_;
    bool                    worker_running_ = false;
    bool                    stop_           = false;
    bool                    busy_           = false;
    bool                    window_active_  = false;
    Clock::time_point       window_end_;
    std::optional<Cycle>    request_;
    uint64_t                next_cycle_ = 1;

    CalibrationCrossScan scan_;
    CalibrationFrame     last_tick_frame_;
    CrossScanStatus      last_tick_status_ = CrossScanStatus::kNone;
};

}

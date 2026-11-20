#include <cerfcore/host/auto_calibration.h>

#include "calibration_frame_source.h"
#include "calibration_run.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/core/log.h>
#include <cerfcore/core/user_config_writer.h>
#include <cerfcore/host/host_presentation.h>
#include <cerfcore/peripherals/cerf_virt/cerf_virt_config_apply.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_boot_gate.h>

#include <utility>

namespace cerfcore {

namespace {

using Ms = std::chrono::milliseconds;

constexpr std::chrono::seconds kWindowLength{60};
constexpr Ms                   kTickInterval{500};
constexpr uint32_t             kResolutionDrift = 50;

}

REGISTER_SERVICE(AutoCalibration);

void AutoCalibration::OnReady() {
    host_          = &emu_.Get<CalibrationHost>();
    frames_        = &emu_.Get<CalibrationFrameSource>();
    presentation_  = &emu_.Get<HostPresentation>();
    config_        = &emu_.Get<DeviceConfig>();
    threads_       = &emu_.Get<InstanceThreads>();
    run_           = emu_.TryGet<CalibrationRun>();
    panel_         = emu_.Get<BoardDatabase>().FindBoard(config_->board_id)->lcd_panel_size;
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
        if (emu_.Get<GuestCpuReset>().DeliveredResetWasResume()) return;
        BeginWindow();
    });
    emu_.Get<StateBootGate>().RegisterColdStartListener([this] { BeginWindow(); });
}

void AutoCalibration::OnShutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_          = true;
        window_active_ = false;
        request_.reset();
        wake_.notify_all();
    }
    if (run_) run_->Abort();
    if (worker_.joinable()) worker_.join();
}

bool AutoCalibration::CanPress() const { return run_ != nullptr; }

void AutoCalibration::RegisterResultListener(ResultListener listener) { result_listeners_.Add(std::move(listener)); }

std::optional<uint64_t> AutoCalibration::Request(const CalibrationRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_ || !run_) return std::nullopt;
    if (busy_ || request_) {
        LOG(Cerf, "%s: a calibration cycle is already running\n", TriggerName(request.trigger));
        return std::nullopt;
    }
    LOG(Cerf, "%s: calibration cycle requested\n", TriggerName(request.trigger));
    request_ = Cycle{next_cycle_++, request};
    EnsureWorkerLocked();
    wake_.notify_all();
    return request_->id;
}

void AutoCalibration::BeginWindow() {
    if (!run_) return;
    if (!config_->automatic_calibration) {
        LOG(Cerf, "automatic calibration is off, no boot scan\n");
        return;
    }
    std::optional<Cycle> dropped;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) return;
        window_active_ = true;
        window_end_    = Clock::now() + kWindowLength;
        dropped.swap(request_);
        LOG(Cerf, "scanning for the calibration screen for %lld s\n", static_cast<long long>(kWindowLength.count()));
        EnsureWorkerLocked();
        wake_.notify_all();
    }
    if (!dropped) return;
    LOG(Cerf, "%s: a reset dropped the requested calibration cycle\n", TriggerName(dropped->request.trigger));
    Report(*dropped, CalibrationResult::kAborted);
}

void AutoCalibration::StopWindow() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (window_active_) LOG(Cerf, "boot scan stopped\n");
    window_active_ = false;
}

void AutoCalibration::SetBusy(bool busy) {
    std::lock_guard<std::mutex> lock(mutex_);
    busy_ = busy;
}

AutoCalibration::Cycle AutoCalibration::ClaimBootFind() {
    std::lock_guard<std::mutex> lock(mutex_);
    busy_ = true;
    if (!request_) return Cycle{next_cycle_++, CalibrationRequest{.trigger = CalibrationTrigger::kBootWindow}};
    LOG(Cerf, "%s: the boot scan found the target first and runs the requested cycle\n",
        TriggerName(request_->request.trigger));
    Cycle cycle = *request_;
    request_.reset();
    return cycle;
}

void AutoCalibration::EnsureWorkerLocked() {
    if (worker_running_) return;
    if (worker_.joinable()) worker_.join();
    worker_running_ = true;
    worker_         = threads_->Start([this] { Loop(); });
}

void AutoCalibration::Loop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stop_) {
        if (request_) {
            const Cycle cycle = *request_;
            request_.reset();
            busy_ = true;
            lock.unlock();
            Explicit(cycle);
            lock.lock();
            busy_ = false;
            continue;
        }
        if (!window_active_) break;
        if (Clock::now() >= window_end_) {
            window_active_ = false;
            LOG(Cerf, "no calibration screen within %lld s, the boot scan ends\n",
                static_cast<long long>(kWindowLength.count()));
            break;
        }
        lock.unlock();
        Tick();
        lock.lock();
        wake_.wait_for(lock, kTickInterval, [this] { return stop_ || request_.has_value(); });
    }
    worker_running_ = false;
}

void AutoCalibration::Tick() {
    CalibrationFrame frame;
    if (!frames_->Grab(frame)) return;
    if (frame == last_tick_frame_) return;
    CalibrationCross      cross;
    const CrossScanStatus status = scan_.Find(frame, cross);
    if (status != last_tick_status_)
        LOG(Cerf, "boot scan of the %ux%u screen: %s\n", frame.width, frame.height,
            CalibrationCrossScan::StatusName(status));
    last_tick_status_ = status;
    if (status != CrossScanStatus::kFound) {
        last_tick_frame_ = std::move(frame);
        return;
    }
    last_tick_frame_  = CalibrationFrame{};
    const Cycle cycle = ClaimBootFind();
    StopWindow();
    const uint64_t generation = run_->AbortGeneration();
    if (ResolutionAllows(cycle)) Attempt(cycle, frame, cross, generation);
    else                         Report(cycle, CalibrationResult::kResolutionSwitched);
    SetBusy(false);
}

void AutoCalibration::Explicit(const Cycle& cycle) {
    const CalibrationTrigger trigger    = cycle.request.trigger;
    const uint64_t           generation = run_->AbortGeneration();
    if (!ResolutionAllows(cycle)) {
        Report(cycle, CalibrationResult::kResolutionSwitched);
        return;
    }
    CalibrationFrame frame;
    CalibrationCross cross;
    CrossScanStatus  status = CrossScanStatus::kNone;
    if (frames_->Grab(frame)) status = scan_.Find(frame, cross);
    if (run_->AbortGeneration() != generation) {
        LOG(Cerf, "%s: a reset, a state restore or shutdown stopped the scan\n", TriggerName(trigger));
        Report(cycle, CalibrationResult::kAborted);
        return;
    }
    if (status != CrossScanStatus::kFound) {
        LOG(Cerf, "%s scan of the %ux%u screen: %s\n", TriggerName(trigger), frame.width, frame.height,
            CalibrationCrossScan::StatusName(status));
        Fail(cycle, cycle.request.host_notices, "no calibration target on the screen");
        return;
    }
    if (trigger == CalibrationTrigger::kGuestAdditions) StopWindow();
    Attempt(cycle, frame, cross, generation);
}

void AutoCalibration::Attempt(const Cycle& cycle, const CalibrationFrame& frame, const CalibrationCross& cross,
                              uint64_t generation) {
    LOG(Cerf, "%s found the %s target at (%d,%d), guest turn %d, on the %ux%u screen\n",
        TriggerName(cycle.request.trigger), CalibrationCrossScan::ZoneName(cross.zone), cross.press_x, cross.press_y,
        cross.quarter, frame.width, frame.height);
    const CalibrationAnswer approval = CalibrationApproved(cycle);
    if (!approval.yes) {
        LOG(Cerf, "automatic calibration not approved\n");
        Report(cycle, CalibrationResult::kDeclined);
        return;
    }
    const bool notices = cycle.request.host_notices && approval.host_notices;
    switch (run_->Perform(generation)) {
        case CalibrationOutcome::kDone:
            LOG(Cerf, "automatic calibration finished\n");
            if (notices) host_->CalibrationFinished();
            Report(cycle, CalibrationResult::kFinished);
            return;
        case CalibrationOutcome::kAborted:
            LOG(Cerf, "automatic calibration stopped by a reset, a state restore or shutdown\n");
            Report(cycle, CalibrationResult::kAborted);
            return;
        case CalibrationOutcome::kFailed:
            Fail(cycle, notices, "the calibration run stopped before the last target");
            return;
    }
}

bool AutoCalibration::ResolutionAllows(const Cycle& cycle) {
    if (!config_->guest_additions || !panel_) return true;
    const uint32_t w = presentation_->SurfaceWidth();
    const uint32_t h = presentation_->SurfaceHeight();
    if (w <= panel_->width + kResolutionDrift && h <= panel_->height + kResolutionDrift) return true;
    LOG(Cerf, "the Guest Additions screen is %ux%u, the board panel is %ux%u\n", w, h, panel_->width,
        panel_->height);
    if (PanelResolutionApproved(cycle, panel_->width, panel_->height))
        return SwitchToPanelResolution(panel_->width, panel_->height);
    LOG(Cerf, "the custom resolution stays, calibrating at it\n");
    return true;
}

bool AutoCalibration::SwitchToPanelResolution(uint32_t width, uint32_t height) {
    LOG(Cerf, "switching the Guest Additions resolution to %ux%u and soft resetting\n", width, height);
    const auto previous = config_->Live();
    if (emu_.Get<UserConfigWriter>().WriteConfigurableScreenSize(width, height)) {
        emu_.Get<CerfVirtConfigApply>().Apply(GuestReset::kSoft, previous->board_configurable_screen_width,
                                              previous->board_configurable_screen_height);
        return false;
    }
    LOG(Cerf, "the panel resolution could not be written to the device configuration, calibrating at the custom "
              "resolution\n");
    return true;
}

CalibrationAnswer AutoCalibration::CalibrationApproved(const Cycle& cycle) {
    if (cycle.request.calibrate) {
        LOG(Cerf, "the request answers the calibration question: %s\n", *cycle.request.calibrate ? "yes" : "no");
        return CalibrationAnswer{.yes = *cycle.request.calibrate, .host_notices = cycle.request.host_notices};
    }
    if (config_->calibration_auto_answer) {
        LOG(Cerf, "--auto-calibration approves the calibration\n");
        return CalibrationAnswer{.yes = true, .host_notices = true};
    }
    return host_->ApprovesCalibration();
}

bool AutoCalibration::PanelResolutionApproved(const Cycle& cycle, uint32_t width, uint32_t height) {
    if (cycle.request.panel_resolution) {
        LOG(Cerf, "the request answers the panel resolution question: %s\n",
            *cycle.request.panel_resolution ? "yes" : "no");
        return *cycle.request.panel_resolution;
    }
    if (config_->calibration_auto_answer) {
        LOG(Cerf, "--auto-calibration approves the panel resolution\n");
        return true;
    }
    return host_->ApprovesPanelResolution(width, height).yes;
}

void AutoCalibration::Fail(const Cycle& cycle, bool host_notices, const char* reason) {
    LOG(Cerf, "%s: automatic calibration failed, %s\n", TriggerName(cycle.request.trigger), reason);
    if (!config_->calibration_auto_answer && host_notices) host_->CalibrationFailed();
    Report(cycle, CalibrationResult::kFailed);
}

void AutoCalibration::Report(const Cycle& cycle, CalibrationResult result) {
    result_listeners_.Call(cycle.id, cycle.request.trigger, result);
}

const char* AutoCalibration::TriggerName(CalibrationTrigger trigger) {
    switch (trigger) {
        case CalibrationTrigger::kBootWindow:     return "boot scan";
        case CalibrationTrigger::kRequest:        return "request";
        case CalibrationTrigger::kGuestAdditions: return "Guest Additions";
    }
    return "boot scan";
}

}

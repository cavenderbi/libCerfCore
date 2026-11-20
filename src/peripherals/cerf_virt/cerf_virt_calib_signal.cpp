#include <cerfcore/peripherals/cerf_virt/cerf_virt_addr_map.h>
#include "cerf_virt_calib_signal_regs.h"
#include "guest_additions_device.h"

#include <cerfcore/peripherals/peripheral_base.h>
#include "../peripheral_dispatcher.h"
#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/auto_calibration.h>
#include <cerfcore/host/calibration_host.h>
#include <cerfcore/host/machine_guest_calibration.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/host/pointer_input.h>
#include <cerfcore/host/pointer_router.h>
#include <cerfcore/host/pointer_source.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/emulation_freeze.h>
#include <cerfcore/state/state_stream.h>

#include <mutex>
#include <string>

namespace cerfcore {

namespace {

class CerfVirtCalibSignal : public Peripheral, public MachineGuestCalibration {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<DeviceConfig>().guest_additions;
    }

    void OnReady() override {
        config_      = &emu_.Get<DeviceConfig>();
        host_        = &emu_.Get<CalibrationHost>();
        calibration_ = &emu_.Get<AutoCalibration>();
        calibration_->RegisterResultListener([this](uint64_t, CalibrationTrigger trigger, CalibrationResult result) {
            if (trigger == CalibrationTrigger::kGuestAdditions && result == CalibrationResult::kFailed) OfferFallback();
        });
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            if (emu_.Get<GuestCpuReset>().DeliveredResetWasResume()) return;
            bool was_shown = false;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                was_shown       = shown_;
                shown_          = false;
                stock_switched_ = false;
            }
            if (!was_shown) return;
            Changed();
            host_->GuestCalibrationEnded(false);
        });
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<MachineObjectTree>().Add(this, &emu_.Get<GuestAdditionsDevice>());
    }

    uint32_t MmioBase() const override {
        return emu_.Get<BoardContext>().GuestAdditionsWindowBase() +
               CerfVirt::kCalibSignalOffset;
    }
    uint32_t MmioSize() const override { return CerfVirt::kCalibSignalSize; }

    void WriteWord(uint32_t addr, uint32_t value) override {
        if (addr - MmioBase() == CerfVirt::kCalSigEvent) {
            if (value == CerfVirt::kCalSigAppeared) {
                Appeared();
                return;
            }
            if (value == CerfVirt::kCalSigDisappeared) {
                Disappeared();
                return;
            }
        }
        HaltUnsupportedAccess("WriteWord", addr, value);
    }

    void SaveState(StateWriter& w) override {
        std::lock_guard<std::mutex> lock(mtx_);
        w.Write<uint8_t>("calibration_window_shown", shown_ ? 1u : 0u);
        w.Write<uint8_t>("calibration_stock_pointer", stock_switched_ ? 1u : 0u);
    }

    void RestoreState(StateReader& r) override {
        uint8_t shown = 0, stock = 0;
        r.Read("calibration_window_shown", shown);
        r.Read("calibration_stock_pointer", stock);
        bool ended   = false;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            ended           = shown_ && shown == 0;
            changed         = shown_ != (shown != 0);
            shown_          = shown != 0;
            stock_switched_ = stock != 0;
        }
        if (changed) Changed();
        if (ended) host_->GuestCalibrationEnded(false);
    }

    std::string ObjectId() const override { return "calibration_screen"; }
    static constexpr char kObjectName[] = "Calibration screen";
    std::string ObjectName() const override { return kObjectName; }

    bool Shown() const override {
        std::lock_guard<std::mutex> lock(mtx_);
        return shown_;
    }

    void SwitchToStockPointer() override {
        auto                        frozen = emu_.Get<EmulationFreeze>().WorkerSection();
        std::lock_guard<std::mutex> lock(mtx_);
        if (!shown_) {
            LOG(GuestAdditions, "[CALIB] the guest calibration window already closed, the pointer source stays\n");
            return;
        }
        auto&                router = emu_.Get<PointerRouter>();
        const PointerSource* guest  = &emu_.Get<PointerInput>();
        for (PointerSource* source : router.Sources()) {
            if (source == guest) continue;
            router.SetActive(source);
            stock_switched_ = true;
            LOG(GuestAdditions, "[CALIB] switched to stock input on user request\n");
            return;
        }
        LOG(Caution, "[CALIB] no stock pointing device to switch to\n");
    }

private:
    void Appeared() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (shown_) return;
            shown_ = true;
        }
        LOG(GuestAdditions, "[CALIB] guest calibration window appeared\n");
        Changed();
        if (!calibration_->CanPress() || !config_->automatic_calibration) {
            OfferFallback();
            return;
        }
        calibration_->Request(CalibrationRequest{.trigger = CalibrationTrigger::kGuestAdditions});
    }

    void Disappeared() {
        bool switched_back = false;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!shown_) return;
            shown_ = false;
            LOG(GuestAdditions, "[CALIB] guest calibration window disappeared\n");
            if (stock_switched_) switched_back = SwitchBackLocked();
        }
        Changed();
        host_->GuestCalibrationEnded(switched_back);
    }

    bool SwitchBackLocked() {
        stock_switched_ = false;
        auto& router    = emu_.Get<PointerRouter>();
        router.RearmAutoSelect();
        if (router.Active() != &emu_.Get<PointerInput>()) {
            LOG(GuestAdditions, "[CALIB] calibration over; the Guest Additions pointer is not ready, keeping the "
                                "stock device\n");
            return false;
        }
        LOG(GuestAdditions, "[CALIB] switched back to the Guest Additions pointer\n");
        return true;
    }

    void OfferFallback() {
        if (config_->calibration_auto_answer || !Shown()) return;
        LOG(GuestAdditions, "[CALIB] the guest calibration screen is left to the frontend\n");
        host_->GuestCalibrationUnhandled();
    }

    DeviceConfig*      config_      = nullptr;
    CalibrationHost*   host_        = nullptr;
    AutoCalibration*   calibration_ = nullptr;
    mutable std::mutex mtx_;
    bool               shown_          = false;
    bool               stock_switched_ = false;
};

REGISTER_SERVICE(CerfVirtCalibSignal);

}

}

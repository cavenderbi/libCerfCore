#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_indicators.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_led_enable.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace cerfcore {

namespace {

constexpr uint32_t  kOnTime         = 0x10000268u;
constexpr uint16_t  kTimeBits       = 0x00FFu;
constexpr uint16_t  kUnits          = 0x007Fu;
constexpr uint32_t  kUnitsPerSecond = 16u;
constexpr ObjectRgb kLedColor{120, 255, 140};
constexpr char      kObjectName[] = "Notification LED";

class CasioCassiopeiaA10NotificationLed : public Peripheral, public MachineLamp {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        enable_ = &emu_.Get<CasioCassiopeiaA10LedEnable>();
        enable_->AddChangeListener([this] { Refresh(); });
        GuestCpuReset& reset = emu_.Get<GuestCpuReset>();
        reset.RegisterResetListener([this](ResetLineKind) {
            std::lock_guard<std::mutex> lk(mutex_);
            on_time_  = 0u;
            off_time_ = 0u;
        });
        reset.RegisterResetReleaseListener([this] { Refresh(); });
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<MachineObjectTree>().Add(this);
    }

    uint32_t MmioBase() const override { return kOnTime; }
    uint32_t MmioSize() const override { return 4u; }

    uint16_t ReadHalf(uint32_t addr) override {
        std::lock_guard<std::mutex> lk(mutex_);
        return addr == kOnTime ? on_time_ : off_time_;
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        if ((value & ~kTimeBits) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
        {
            std::lock_guard<std::mutex> lk(mutex_);
            (addr == kOnTime ? on_time_ : off_time_) = value;
        }
        Refresh();
    }

    void SaveState(StateWriter& w) override {
        std::lock_guard<std::mutex> lk(mutex_);
        w.Write("on_time", on_time_);
        w.Write("off_time", off_time_);
    }

    void RestoreState(StateReader& r) override {
        std::lock_guard<std::mutex> lk(mutex_);
        r.Read("on_time", on_time_);
        r.Read("off_time", off_time_);
    }

    void PostRestore() override { Refresh(); }

    std::string ObjectId() const override { return "notification_led"; }
    std::string ObjectName() const override { return kObjectName; }

    LampState Lamp() const override {
        uint16_t on  = 0u;
        uint16_t off = 0u;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            on  = on_time_;
            off = off_time_;
        }
        const CasioCassiopeiaA10LedMode mode = enable_->Mode(on);
        if (mode == CasioCassiopeiaA10LedMode::kOff) return LampState{false, false, kLedColor};
        if (mode == CasioCassiopeiaA10LedMode::kSteady) return LampState{true, false, kLedColor};
        return LampState{true, true, kLedColor, Milliseconds(on), Milliseconds(off)};
    }

private:
    static uint32_t Milliseconds(uint16_t time) { return ((time & kUnits) + 1u) * 1000u / kUnitsPerSecond; }

    CasioCassiopeiaA10LedEnable* enable_ = nullptr;
    mutable std::mutex           mutex_;
    uint16_t                     on_time_  = 0u;
    uint16_t                     off_time_ = 0u;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10NotificationLed);

}

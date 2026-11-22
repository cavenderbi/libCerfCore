#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/machine_indicators.h>
#include <cerfcore/host/machine_object_tree.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include <cerfcore/boards/board_context.h>
#include "samsung_smdk2410_family_id.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x500FFFD0u;
constexpr uint32_t kSize = 0x00000006u;
constexpr uint32_t kNumLeds      = 2u;
constexpr uint32_t kBytesPerLed  = 3u;

constexpr uint32_t kFieldOnOffBlink = 0u;
constexpr uint32_t kFieldOnTime     = 1u;
constexpr uint32_t kFieldOffTime    = 2u;

constexpr uint8_t kOff   = 0u;
constexpr uint8_t kOn     = 1u;
constexpr uint8_t kBlink  = 2u;

constexpr uint32_t kDefaultBlinkTicks = 5u;
constexpr uint32_t kTickMs            = 100u;

constexpr ObjectRgb kLedColor{78, 201, 90};

class SamsungSmdk2410NotificationLed : public Peripheral, public MachineLamp {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::SamsungSmdk2410;
    }
    void OnReady() override {
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            std::lock_guard<std::mutex> lk(state_mutex_);
            regs_.fill(0u);
        });
        emu_.Get<PeripheralDispatcher>().Register(this);
        auto& tree = emu_.Get<MachineObjectTree>();
        tree.Add(this);
        tree.Add(&vibrator_, this);
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    uint8_t ReadByte (uint32_t addr) override;
    void    WriteByte(uint32_t addr, uint8_t value) override;

    void SaveState(StateWriter& w) override {
        std::lock_guard<std::mutex> lk(state_mutex_);
        w.WriteBytes("regs", regs_.data(), regs_.size());
    }
    void RestoreState(StateReader& r) override {
        std::lock_guard<std::mutex> lk(state_mutex_);
        r.ReadBytes("regs", regs_.data(), regs_.size());
    }

    std::string ObjectId() const override { return "notification_led"; }
    static constexpr char kObjectName[] = "Notification LED";
    std::string ObjectName() const override { return kObjectName; }
    LampState   Lamp() const override;

private:
    class Vibrator final : public MachineVibrator {
    public:
        Vibrator(SamsungSmdk2410NotificationLed& owner, CerfEmulator& emu) : MachineVibrator(emu), owner_(owner) {}
        std::string ObjectId() const override { return "vibrator"; }
        static constexpr char kObjectName[] = "Vibrator";
        std::string ObjectName() const override { return kObjectName; }
        bool        Vibrating() const override { return owner_.Mode(1) != kOff; }

        using MachineVibrator::Refresh;

    private:
        SamsungSmdk2410NotificationLed& owner_;
    };

    static const char* FieldName(uint32_t led_off);
    uint8_t Mode(uint32_t led) const;
    void    RefreshAll();

    mutable std::mutex                            state_mutex_;
    std::array<uint8_t, kNumLeds * kBytesPerLed>  regs_{};
    Vibrator                                      vibrator_{*this, emu_};
};

uint8_t SamsungSmdk2410NotificationLed::Mode(uint32_t led) const {
    std::lock_guard<std::mutex> lk(state_mutex_);
    return regs_[led * kBytesPerLed + kFieldOnOffBlink];
}

void SamsungSmdk2410NotificationLed::RefreshAll() {
    Refresh();
    vibrator_.Refresh();
}

const char* SamsungSmdk2410NotificationLed::FieldName(uint32_t led_off) {
    switch (led_off) {
        case kFieldOnOffBlink: return "OnOffBlink";
        case kFieldOnTime:     return "OnTime";
        case kFieldOffTime:    return "OffTime";
        default:               return "?";
    }
}

uint8_t SamsungSmdk2410NotificationLed::ReadByte(uint32_t addr) {
    const uint32_t off = addr - kBase;
    uint8_t value = 0;
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (off >= regs_.size()) {
            HaltUnsupportedAccess("ReadByte", addr, 0);  /* noreturn */
        }
        value = regs_[off];
    }
    const uint32_t led     = off / kBytesPerLed;
    const uint32_t led_off = off % kBytesPerLed;
    LOG(Periph, "[NLED] read8 led=%u %s -> 0x%02X\n",
        led, FieldName(led_off), value);
    return value;
}

void SamsungSmdk2410NotificationLed::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off     = addr - kBase;
    const uint32_t led     = off / kBytesPerLed;
    const uint32_t led_off = off % kBytesPerLed;
    LOG(Periph, "[NLED] write8 led=%u %s = 0x%02X\n",
        led, FieldName(led_off), value);
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (off >= regs_.size()) {
            HaltUnsupportedAccess("WriteByte", addr, value);
        }
        regs_[off] = value;
    }
    RefreshAll();
}

LampState SamsungSmdk2410NotificationLed::Lamp() const {
    uint8_t mode0, on0, off0;
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        mode0 = regs_[kFieldOnOffBlink];
        on0   = regs_[kFieldOnTime];
        off0  = regs_[kFieldOffTime];
    }
    if (mode0 != kBlink) return LampState{mode0 == kOn, false, kLedColor};
    uint32_t on_ticks  = on0;
    uint32_t off_ticks = off0;
    if (on_ticks == 0 && off_ticks == 0) on_ticks = off_ticks = kDefaultBlinkTicks;
    return LampState{true, true, kLedColor, on_ticks * kTickMs, off_ticks * kTickMs};
}

}  /* namespace */

REGISTER_SERVICE(SamsungSmdk2410NotificationLed);

}

#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "hp_jornada_540_audio_out.h"
#include "hp_jornada_540_battery.h"
#include "hp_jornada_540_capture_timer.h"
#include "hp_jornada_540_cf_socket.h"
#include "hp_jornada_540_family_id.h"
#include "hp_jornada_540_key_wake.h"
#include "hp_jornada_540_lcd_clock.h"
#include "hp_jornada_540_notification_led.h"

#include <array>
#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kAsicPa    = 0x12000000u;
constexpr uint32_t kAsicBytes = 0x1000u;

struct OutputLatchPorts {
    uint32_t set;
    uint32_t clear;
    bool     clear_reads_complement;
};

constexpr std::array<OutputLatchPorts, 3> kLatches{{
    {0x24u, 0x34u, false},
    {0x6Cu, 0x68u, true},
    {0x30u, 0x00u, true},
}};

constexpr size_t   kAudioLatch          = 1u;
constexpr uint16_t kFifoReset           = 0x0080u;
constexpr size_t   kRunningLatch        = 1u;
constexpr uint16_t kRunning             = 0x0002u;
constexpr size_t   kDisplayLatch        = 2u;
constexpr uint16_t kDisplayPower        = 0x0002u;
constexpr size_t   kCfLatch             = 2u;
constexpr uint16_t kCfPower             = 0x0100u;
constexpr uint32_t kCommandOffset    = 0x04u;
constexpr uint32_t kStatusOffset     = 0x08u;
constexpr uint32_t kCaptureDivisorOffset = 0x10u;
constexpr uint32_t kClockOffset      = 0x14u;
constexpr uint32_t kBlinkCountOffset = 0x1Cu;
constexpr uint32_t kAudioControlOffset = 0x44u;
constexpr uint32_t kWrittenWordsOffset = 0x74u;
constexpr uint32_t kPlayedWordsOffset  = 0x78u;
constexpr uint32_t kSampleOffset       = 0x7Cu;
constexpr uint32_t kAudioModeOffset    = 0x80u;
constexpr uint32_t kFrameDivisorOffset = 0x84u;
constexpr uint32_t kRegister94Offset = 0x94u;
constexpr uint32_t kRegister98Offset = 0x98u;
constexpr uint32_t kRegister9cOffset = 0x9Cu;
constexpr uint16_t kLedStart         = 0x0010u;
constexpr uint16_t kLedStop          = 0x0020u;
constexpr uint16_t kComparatorStart  = 0x4000u;
constexpr uint16_t kKeyWakeAck       = 0x2000u;
constexpr uint16_t kCommandPulses    = 0x8A8Au;
constexpr uint16_t kCommandBits      = kLedStart | kLedStop | kComparatorStart | kKeyWakeAck | kCommandPulses;
constexpr uint16_t kCaptureControlBits = 0x0038u;
constexpr uint16_t kLcdDividerBits     = 0x01C0u;
constexpr uint16_t kClockBits          = kCaptureControlBits | kLcdDividerBits;
constexpr uint16_t kOutputEnable       = 0x0002u;
constexpr uint16_t kAudioMode          = 0x0040u;
constexpr uint16_t kStatusCfAbsent   = 0x0001u;
constexpr uint16_t kStatusLedIdle    = 0x0020u;
constexpr uint16_t kStatusCharging   = 0x0100u;
constexpr uint16_t kStatusAcAbsent   = 0x0400u;
constexpr uint16_t kStatusKeyWake    = 0x0800u;

class HpJornada540Asic : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

    void OnReady() override {
        led_       = &emu_.Get<HpJornada540NotificationLed>();
        battery_   = &emu_.Get<HpJornada540Battery>();
        lcd_clock_ = &emu_.Get<HpJornada540LcdClock>();
        audio_out_ = &emu_.Get<HpJornada540AudioOut>();
        capture_   = &emu_.Get<HpJornada540CaptureTimer>();
        key_wake_  = &emu_.Get<HpJornada540KeyWake>();
        cf_socket_ = &emu_.Get<HpJornada540CfSocket>();
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
            if (emu_.Get<GuestCpuReset>().PowerOnReset(kind)) PowerOnReset();
        });
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return kAsicPa; }
    uint32_t MmioSize() const override { return kAsicBytes; }

    uint16_t ReadHalf(uint32_t addr) override {
        const uint32_t off = addr - kAsicPa;
        for (size_t i = 0; i < kLatches.size(); ++i) {
            if (off == kLatches[i].clear && kLatches[i].clear_reads_complement) {
                return static_cast<uint16_t>(~levels_[i]);
            }
        }
        switch (off) {
            case kStatusOffset:     return Status();
            case kClockOffset:      return clock_;
            case kBlinkCountOffset: return led_->BlinkCount();
            case kAudioControlOffset: return audio_control_;
            case kWrittenWordsOffset: return audio_out_->WrittenWords();
            case kPlayedWordsOffset:  return audio_out_->PlayedWords();
            case kRegister94Offset: return register94_;
            case kRegister98Offset: return 0u;
            case kRegister9cOffset: return 0u;
            default:                HaltUnsupportedAccess("ReadHalf", addr, 0u);
        }
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t off = addr - kAsicPa;
        if (off == kCommandOffset) {
            WriteCommand(addr, value);
            return;
        }
        if (off == kBlinkCountOffset) {
            led_->WriteBlinkCount(value);
            return;
        }
        if (off == kClockOffset) {
            if ((value & ~kClockBits) != 0u) HaltUnsupportedAccess("WriteHalf(clock bits)", addr, value);
            SetClock(value);
            return;
        }
        if (off == kRegister94Offset) {
            register94_ = value;
            return;
        }
        if (WriteAudio(addr, off, value)) return;
        if (off == kRegister98Offset) return;
        for (size_t i = 0; i < kLatches.size(); ++i) {
            if (off == kLatches[i].set) {
                SetLatch(i, static_cast<uint16_t>(levels_[i] | value));
                return;
            }
            if (off == kLatches[i].clear) {
                SetLatch(i, static_cast<uint16_t>(levels_[i] & ~value));
                return;
            }
        }
        HaltUnsupportedAccess("WriteHalf", addr, value);
    }

    void SaveState(StateWriter& w) override {
        w.Write("output_latches", levels_);
        w.Write("register_94", register94_);
        w.Write("clock", clock_);
        w.Write("audio_control", audio_control_);
        led_->SaveState(w);
        audio_out_->SaveState(w);
        capture_->SaveState(w);
        key_wake_->SaveState(w);
        cf_socket_->SaveState(w);
    }

    void RestoreState(StateReader& r) override {
        r.Read("output_latches", levels_);
        r.Read("register_94", register94_);
        uint16_t clock = 0u;
        r.Read("clock", clock);
        ApplyClock(clock);
        r.Read("audio_control", audio_control_);
        led_->RestoreState(r);
        led_->SetDisplayOff(DisplayOffWhileRunning());
        audio_out_->RestoreState(r, (audio_control_ & kOutputEnable) != 0u, FifoResetHeld());
        capture_->RestoreState(r, static_cast<uint16_t>(clock & kCaptureControlBits));
        key_wake_->RestoreState(r);
        cf_socket_->RestoreState(r);
    }

private:
    bool FifoResetHeld() const { return (levels_[kAudioLatch] & kFifoReset) != 0u; }

    bool DisplayOffWhileRunning() const {
        return (levels_[kDisplayLatch] & kDisplayPower) == 0u && (levels_[kRunningLatch] & kRunning) != 0u;
    }

    void SetLatch(size_t i, uint16_t level) {
        levels_[i] = level;
        if (i == kAudioLatch) audio_out_->SetFifoReset(FifoResetHeld());
        if (i == kCfLatch) cf_socket_->SetCardPower((level & kCfPower) != 0u);
        led_->SetDisplayOff(DisplayOffWhileRunning());
    }

    void PowerOnReset() {
        levels_.fill(0u);
        cf_socket_->SetCardPower(false);
        register94_    = 0u;
        audio_control_ = 0u;
        ApplyClock(0u);
        led_->Stop();
        led_->WriteBlinkCount(0u);
        led_->SetDisplayOff(DisplayOffWhileRunning());
        audio_out_->PowerOnReset();
        capture_->PowerOnReset();
        key_wake_->Clear();
    }

    void SetClock(uint16_t value) {
        ApplyClock(value);
        capture_->SetControl(static_cast<uint16_t>(value & kCaptureControlBits));
    }

    void ApplyClock(uint16_t value) {
        clock_ = value;
        lcd_clock_->SetDivider(((value >> 6) & 0x7u) + 1u);
    }

    bool WriteAudio(uint32_t addr, uint32_t off, uint16_t value) {
        switch (off) {
            case kCaptureDivisorOffset: capture_->SetDivisor(value); return true;
            case kSampleOffset:         audio_out_->WriteSample(value); return true;
            case kFrameDivisorOffset:   audio_out_->SetFrameDivisor(value); return true;
            case kAudioControlOffset:
                if ((value & ~kOutputEnable) != 0u) HaltUnsupportedAccess("WriteHalf(audio control bits)", addr, value);
                audio_control_ = value;
                audio_out_->SetEnabled((value & kOutputEnable) != 0u);
                return true;
            case kAudioModeOffset:
                if (value != kAudioMode) HaltUnsupportedAccess("WriteHalf(audio mode)", addr, value);
                return true;
            default: return false;
        }
    }

    uint16_t Status() const {
        uint16_t v = cf_socket_->CardAbsent() ? kStatusCfAbsent : 0u;
        if (!led_->Blinking()) v = static_cast<uint16_t>(v | kStatusLedIdle);
        if (battery_->Charging()) v = static_cast<uint16_t>(v | kStatusCharging);
        if (!battery_->OnExternalPower()) v = static_cast<uint16_t>(v | kStatusAcAbsent);
        if (key_wake_->Pending()) v = static_cast<uint16_t>(v | kStatusKeyWake);
        return v;
    }

    void WriteCommand(uint32_t addr, uint16_t value) {
        const bool start = (value & kLedStart) != 0u;
        const bool stop  = (value & kLedStop) != 0u;
        if ((value & ~kCommandBits) != 0u) HaltUnsupportedAccess("WriteHalf(command bits)", addr, value);
        if (start && stop) HaltUnsupportedAccess("WriteHalf starts and stops the notification LED", addr, value);
        if (start) led_->Start();
        if (stop) led_->Stop();
        if ((value & kKeyWakeAck) != 0u) key_wake_->Clear();
    }

    HpJornada540NotificationLed*          led_       = nullptr;
    HpJornada540Battery*                  battery_   = nullptr;
    HpJornada540LcdClock*                 lcd_clock_ = nullptr;
    HpJornada540AudioOut*                 audio_out_ = nullptr;
    HpJornada540CaptureTimer*             capture_   = nullptr;
    HpJornada540KeyWake*                  key_wake_  = nullptr;
    HpJornada540CfSocket*                 cf_socket_ = nullptr;
    std::array<uint16_t, kLatches.size()> levels_{};
    uint16_t                              register94_    = 0u;
    uint16_t                              clock_         = 0u;
    uint16_t                              audio_control_ = 0u;
};

}

REGISTER_SERVICE(HpJornada540Asic);

}

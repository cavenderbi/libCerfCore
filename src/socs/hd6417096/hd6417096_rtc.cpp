#include "../sh77xx/sh77xx_rtc_oscillator.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../peripherals/rtc/bcd_calendar.h"
#include "../sh77xx/sh77xx_intc.h"
#include "hd6417096_clock_seed.h"
#include "hd6417096_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kClockBase   = 0x1FFFFEC0u;
constexpr uint32_t kClockBytes  = 0x20u;
constexpr uint32_t kTime        = 0x1FFFFEC2u;
constexpr uint32_t kAlarm       = 0x1FFFFEC8u;
constexpr uint32_t kControl     = 0x1FFFFECEu;
constexpr uint32_t kFlags       = 0x1FFFFED0u;
constexpr uint32_t kFlagSelect  = 0x1FFFFED2u;
constexpr uint32_t kFields      = 3u;

constexpr uint32_t kMinuteSecond = 0u;
constexpr uint32_t kDayHour      = 1u;

constexpr uint16_t kAlarmEnable  = 0x0080u;
constexpr uint16_t kHold         = 0x0100u;
constexpr uint16_t kControlBits  = kAlarmEnable | kHold;
constexpr uint16_t kAlarmFlag    = 0x0001u;
constexpr uint16_t kFlagBits     = 0x000Fu;
constexpr uint16_t kNoFlagSelect = 0x0000u;
constexpr uint8_t  kAllFields    = 0x07u;

constexpr uint64_t kSecondsPerDay  = 86400u;
constexpr uint32_t kSecondsPerHour = 3600u;
constexpr uint32_t kSecondsPerMin  = 60u;
constexpr uint64_t kDayLimit       = 100000u;
constexpr uint32_t kDayHighMax     = 9u;

uint16_t Pack(uint32_t high, uint32_t low) { return static_cast<uint16_t>((high << 8) | low); }

uint16_t Encode(uint32_t field, uint64_t total) {
    const uint64_t days = total / kSecondsPerDay;
    const uint32_t time = static_cast<uint32_t>(total % kSecondsPerDay);
    if (field == kMinuteSecond) {
        return Pack(BcdCalendar::ToBcd(time / kSecondsPerMin % 60u), BcdCalendar::ToBcd(time % kSecondsPerMin));
    }
    if (field == kDayHour) {
        return Pack(BcdCalendar::ToBcd(static_cast<uint32_t>(days % 100u)), BcdCalendar::ToBcd(time / kSecondsPerHour));
    }
    return Pack(static_cast<uint32_t>(days / 10000u), BcdCalendar::ToBcd(static_cast<uint32_t>(days / 100u % 100u)));
}

bool Apply(uint32_t field, uint16_t value, uint64_t& total) {
    uint64_t       days   = total / kSecondsPerDay;
    const uint32_t time   = static_cast<uint32_t>(total % kSecondsPerDay);
    uint32_t       hour   = time / kSecondsPerHour;
    uint32_t       minute = time / kSecondsPerMin % 60u;
    uint32_t       second = time % kSecondsPerMin;
    const uint32_t high   = value >> 8;
    const uint32_t low    = value & 0xFFu;
    uint32_t       digits = 0u;
    if (field == kMinuteSecond) {
        if (!BcdCalendar::DecodeBcd(high, 0u, 59u, minute) || !BcdCalendar::DecodeBcd(low, 0u, 59u, second)) return false;
    } else if (field == kDayHour) {
        if (!BcdCalendar::DecodeBcd(high, 0u, 99u, digits) || !BcdCalendar::DecodeBcd(low, 0u, 23u, hour)) return false;
        days = days / 100u * 100u + digits;
    } else {
        if (high > kDayHighMax || !BcdCalendar::DecodeBcd(low, 0u, 99u, digits)) return false;
        days = high * 10000u + digits * 100u + days % 100u;
    }
    total = days * kSecondsPerDay + hour * kSecondsPerHour + minute * kSecondsPerMin + second;
    return true;
}

class Hd6417096Rtc : public Sh77xxRtcOscillator {
public:
    using Sh77xxRtcOscillator::Sh77xxRtcOscillator;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }

    void OnReady() override {
        intc_ = &emu_.Get<Sh77xxIntc>();
        AttachOscillator();
        AttachDivider(kCrystalHz);
        seconds_ = emu_.Get<Hd6417096ClockSeed>().PowerOnSeconds();
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { OnReset(); });
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return kClockBase; }
    uint32_t MmioSize() const override { return kClockBytes; }

    uint16_t ReadHalf(uint32_t addr) override {
        Advance();
        uint16_t value = 0u;
        if (addr >= kTime && addr < kAlarm && (addr & 1u) == 0u) {
            value = Encode((addr - kTime) / 2u, seconds_);
        } else if (addr == kControl) {
            value = control_;
        } else {
            HaltUnsupportedAccess("ReadHalf", addr, 0u);
        }
        Settle();
        return value;
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        Advance();
        if (addr >= kTime && addr < kAlarm && (addr & 1u) == 0u) {
            WriteTime(addr, value);
        } else if (addr >= kAlarm && addr < kControl && (addr & 1u) == 0u) {
            WriteAlarm(addr, value);
        } else if (addr == kControl) {
            WriteControl(addr, value);
        } else if (addr == kFlags) {
            if ((value & ~kFlagBits) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
            flags_ = static_cast<uint16_t>(flags_ & ~value);
        } else if (addr != kFlagSelect || value != kNoFlagSelect) {
            HaltUnsupportedAccess("WriteHalf", addr, value);
        }
        Settle();
    }

    void SaveState(StateWriter& w) override {
        Advance();
        SaveOscillator(w);
        SaveDivider(w);
        w.Write("seconds", seconds_);
        w.Write("control", control_);
        w.Write("flags", flags_);
        w.Write("alarm", alarm_);
        w.Write("alarm_written", alarm_written_);
        w.Write("alarm_seconds", alarm_seconds_);
    }

    void RestoreState(StateReader& r) override {
        RestoreOscillator(r);
        RestoreDivider(r);
        r.Read("seconds", seconds_);
        r.Read("control", control_);
        r.Read("flags", flags_);
        r.Read("alarm", alarm_);
        r.Read("alarm_written", alarm_written_);
        r.Read("alarm_seconds", alarm_seconds_);
    }

    void PostRestore() override { Settle(); }

protected:
    void OnOscillatorRateChange() override {
        Advance();
        Settle();
    }

    void OnDividerEvent() override {
        Advance();
        Settle();
    }

    uint64_t NextWakingTick() override {
        return intc_->WakesStandby(Sh77xxIntcSource::kAti) ? NextAlarmTick() : kNoTick;
    }

private:
    bool Held() const { return (control_ & kHold) != 0u; }
    bool AlarmOn() const { return (control_ & kAlarmEnable) != 0u; }

    void Advance() {
        const uint64_t secs = StepDivider().Seconds();
        if (secs == 0u || Held()) return;
        const uint64_t from = seconds_;
        seconds_ += secs;
        if (seconds_ >= kDayLimit * kSecondsPerDay) {
            emu_.Get<Fatal>().Die("Hd6417096Rtc: the day count passes %llu", static_cast<unsigned long long>(kDayLimit - 1u));
        }
        if (AlarmOn() && from < alarm_seconds_ && alarm_seconds_ <= seconds_) flags_ |= kAlarmFlag;
    }

    void MatchAlarm() {
        if (AlarmOn() && seconds_ == alarm_seconds_) flags_ |= kAlarmFlag;
    }

    void WriteTime(uint32_t addr, uint16_t value) {
        if (!Held()) {
            emu_.Get<Fatal>().Die("Hd6417096Rtc: time write 0x%04X at 0x%08X while the count is not held", value, addr);
        }
        if (!Apply((addr - kTime) / 2u, value, seconds_)) HaltUnsupportedAccess("WriteHalf", addr, value);
        MatchAlarm();
    }

    void WriteAlarm(uint32_t addr, uint16_t value) {
        const uint32_t field = (addr - kAlarm) / 2u;
        alarm_[field]        = value;
        alarm_written_       = static_cast<uint8_t>(alarm_written_ | (1u << field));
        if (AlarmOn()) LoadAlarm();
    }

    void WriteControl(uint32_t addr, uint16_t value) {
        if ((value & ~kControlBits) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
        const bool release = Held() && (value & kHold) == 0u;
        const bool enable  = !AlarmOn() && (value & kAlarmEnable) != 0u;
        control_           = value;
        if (release) RestartDivider();
        if (enable) LoadAlarm();
    }

    void LoadAlarm() {
        if (alarm_written_ != kAllFields) {
            emu_.Get<Fatal>().Die("Hd6417096Rtc: alarm enabled with alarm fields 0x%X unwritten",
                                  kAllFields & ~alarm_written_);
        }
        uint64_t target = 0u;
        for (uint32_t field = 0; field < kFields; ++field) {
            if (!Apply(field, alarm_[field], target)) {
                emu_.Get<Fatal>().Die("Hd6417096Rtc: alarm field %u holds 0x%04X outside its range", field, alarm_[field]);
            }
        }
        alarm_seconds_ = target;
        MatchAlarm();
    }

    uint64_t NextAlarmTick() const {
        if (!AlarmOn() || Held() || (flags_ & kAlarmFlag) != 0u || alarm_seconds_ <= seconds_) return kNoTick;
        return SecondBoundaryTick(alarm_seconds_ - seconds_);
    }

    void Settle() {
        intc_->SetSourceRequest(Sh77xxIntcSource::kAti, AlarmOn() && (flags_ & kAlarmFlag) != 0u);
        ScheduleTick(NextAlarmTick());
    }

    void OnReset() {
        Advance();
        control_ = 0u;
        flags_   = 0u;
        Settle();
    }

    Sh77xxIntc* intc_          = nullptr;
    uint64_t    seconds_       = 0u;
    uint64_t    alarm_seconds_ = 0u;
    uint16_t    control_       = 0u;
    uint16_t    flags_         = 0u;
    uint16_t    alarm_[kFields]{};
    uint8_t     alarm_written_ = 0u;
};

}

REGISTER_SERVICE_AS(Hd6417096Rtc, Sh77xxRtcOscillator);

}

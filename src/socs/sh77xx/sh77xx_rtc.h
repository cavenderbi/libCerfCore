#pragma once

#include "sh77xx_rtc_oscillator.h"

#include "sh77xx_intc.h"
#include "sh77xx_rtc_calendar.h"
#include "sh77xx_standby_control.h"

#include <cstdint>

namespace cerfcore {

enum class ResetLineKind;

class Sh77xxRtc : public Sh77xxRtcOscillator {
public:
    using Sh77xxRtcOscillator::Sh77xxRtcOscillator;

    void OnReady() override;

    uint32_t MmioBase() const override { return 0x1FFFFEC0u; }
    uint32_t MmioSize() const override { return 0x20u; }

    uint8_t ReadByte(uint32_t addr) override;
    void    WriteByte(uint32_t addr, uint8_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

protected:
    void     OnOscillatorRateChange() override;
    void     OnDividerEvent() override;
    uint64_t NextWakingTick() override;

private:
    void     RequireClock(const char* access, uint32_t addr, uint8_t value) const;
    void     RequireCountable(const char* trigger) const;
    void     Advance();
    void     CountSeconds(uint64_t secs);
    void     Compare();
    void     Settle();
    void     Schedule();
    uint64_t NextEventTick(bool waking_only);
    void     RecomputeAlarmDue();
    void     DriveLines();
    void     OnReset(ResetLineKind kind);
    void     PowerOnReset();
    void     ManualReset();
    uint8_t  ReadRcr1() const;
    uint8_t  ReadRcr2() const;
    void     WriteRcr1(uint8_t value);
    void     WriteRcr2(uint8_t value);
    void     WriteCounter(Sh77xxRtcCalendar::Counter counter, uint8_t value);
    void     WriteAlarm(uint32_t field, uint8_t value);
    uint64_t PeriodTicks() const;

    Sh77xxIntc*              intc_  = nullptr;
    const Sh77xxStbcr*       stbcr_ = nullptr;
    Sh77xxRtcCalendar        cal_;
    Sh77xxRtcCalendar::Alarm alarm_{};
    uint64_t                 alarm_due_       = 0u;
    bool                     alarm_stale_     = true;
    bool                     alarm_reachable_ = false;
    uint8_t                  pes_   = 0u;
    bool                     cf_    = false;
    bool                     cie_   = false;
    bool                     aie_   = false;
    bool                     af_    = false;
    bool                     pef_   = false;
    bool                     start_ = true;
    bool                     match_ = false;
};

}

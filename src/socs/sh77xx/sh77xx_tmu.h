#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include "../../execution/guest_cycle_clock.h"
#include "../reload_down_counter.h"
#include "sh77xx_intc.h"
#include "sh77xx_standby_control.h"
#include "sh77xx_standby_entry.h"
#include "sh77xx_timer_clock.h"

#include <cstdint>

namespace cerfcore {

class Sh77xxTmu : public Peripheral {
public:
    using Peripheral::Peripheral;

    void OnReady() override;

    uint32_t MmioBase() const override { return 0x1FFFFE90u; }
    uint32_t MmioSize() const override { return 0x2Cu; }

    uint8_t  ReadByte(uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t value) override;
    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;
    uint32_t ReadWord(uint32_t addr) override;
    void     WriteWord(uint32_t addr, uint32_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

protected:
    virtual bool StandbyInitializesFlags() const = 0;

private:
    static constexpr uint32_t kChannels = 3u;

    struct Channel {
        ReloadDownCounter       counter{ReloadDownCounter::FlagPoint::kReload};
        GuestCycleClock::Event* event   = nullptr;
        uint16_t                control = 0u;
    };

    enum class Field : uint8_t { kConstant, kCount, kControl, kNone };

    Field    Locate(uint32_t addr, Channel*& ch);
    uint32_t Index(const Channel& ch) const;
    bool     Rtcclk(const Channel& ch) const;
    uint64_t Ticks(const Channel& ch);
    void     Arm(Channel& ch);
    void     Drive(Channel& ch);
    void     PublishStandbyWake(const Channel& ch);
    void     Sync(Channel& ch);
    void     RequireClock(const char* access, uint32_t addr, uint32_t value) const;
    uint8_t  ReadTstr() const;
    void     WriteTstr(uint8_t value);
    void     WriteTocr(uint8_t value) const;
    uint16_t ReadControl(Channel& ch);
    void     WriteControl(Channel& ch, uint16_t value);
    void     PowerOnReset();
    void     OnStandbyControlChange(uint8_t from, uint8_t to);
    void     EnterStandby();
    void     StopPeripheralClockChannels();
    void     SyncRtcclkChannels();
    int64_t  ParkWakeDueNs();

    GuestCycleClock* clock_ = nullptr;
    Sh77xxIntc*      intc_  = nullptr;
    Sh77xxStbcr*     stbcr_ = nullptr;
    Sh77xxTimerClock timer_clock_{emu_};
    Channel          ch_[kChannels];
};

}

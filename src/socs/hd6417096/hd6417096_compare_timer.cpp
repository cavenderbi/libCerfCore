#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../execution/guest_cycle_clock.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../cycle_anchored_counter.h"
#include "../sh77xx/sh77xx_intc.h"
#include "../sh77xx/sh77xx_standby_entry.h"
#include "../sh77xx/sh77xx_timer_clock.h"
#include "hd6417096_id.h"

#include <cstdint>
#include <string>

namespace cerfcore {

namespace {

constexpr uint32_t kBase          = 0x1FFFFE20u;
constexpr uint32_t kChannels      = 2u;
constexpr uint32_t kChannelStride = 0x20u;
constexpr uint32_t kOffCounter    = 0x00u;
constexpr uint32_t kOffCompare    = 0x04u;
constexpr uint32_t kOffClear      = 0x10u;
constexpr uint32_t kOffControl    = 0x12u;
constexpr uint32_t kOffSelect     = 0x14u;
constexpr uint32_t kBlockBytes    = kChannelStride + kOffSelect + 2u;

constexpr uint16_t kNoClear      = 0x0000u;
constexpr uint16_t kClearCount   = 0x0001u;
constexpr uint16_t kStop         = 0x0000u;
constexpr uint16_t kRun          = 0x0001u;
constexpr uint16_t kRunInterrupt = 0x0003u;
constexpr uint64_t kCounterSpan  = uint64_t{1} << 32;

constexpr Sh77xxIntcSource kCompareSource[kChannels] = {Sh77xxIntcSource::kIntevt6C0, Sh77xxIntcSource::kIntevt6E0};

constexpr const char* kChannelPrefix[kChannels] = {"ch0_", "ch1_"};

class Hd6417096CompareTimer : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }

    void OnReady() override {
        clock_ = &emu_.Get<GuestCycleClock>();
        intc_  = &emu_.Get<Sh77xxIntc>();
        for (Channel& ch : ch_) {
            Channel* channel = &ch;
            ch.event         = clock_->Add([this, channel] { Update(*channel); });
        }
        timer_clock_.Attach([this] { UpdateAll(); }, [this] { UpdateAll(); });
        emu_.Get<Sh77xxStandbyEntry>().RegisterListener([this] { EnterStandby(); });
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Reset(); });
        Reset();
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kBlockBytes; }

    uint32_t ReadWord(uint32_t addr) override {
        Channel&       ch     = ChannelOf(addr);
        const uint32_t offset = Offset(addr);
        if (offset == kOffCounter) return Count(ch);
        if (offset != kOffCompare || !ch.compare_written) HaltUnsupportedAccess("ReadWord", addr, 0u);
        return ch.compare;
    }

    uint16_t ReadHalf(uint32_t addr) override {
        const Channel& ch = ChannelOf(addr);
        if (Offset(addr) != kOffControl || !ch.control_written) HaltUnsupportedAccess("ReadHalf", addr, 0u);
        return ch.control;
    }

    void WriteWord(uint32_t addr, uint32_t value) override {
        Channel& ch = ChannelOf(addr);
        if (Offset(addr) != kOffCompare) HaltUnsupportedAccess("WriteWord", addr, value);
        ch.compare         = value;
        ch.compare_written = true;
        Update(ch);
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        Channel& ch = ChannelOf(addr);
        switch (Offset(addr)) {
        case kOffClear:
            WriteClear(ch, addr, value);
            return;
        case kOffControl:
            WriteControl(ch, addr, value);
            return;
        case kOffSelect:
            WriteSelect(ch, addr, value);
            return;
        default:
            HaltUnsupportedAccess("WriteHalf", addr, value);
        }
    }

    void SaveState(StateWriter& w) override {
        for (Channel& ch : ch_) {
            const std::string prefix = kChannelPrefix[Index(ch)];
            const uint32_t    count  = Count(ch);
            w.Write((prefix + "count").c_str(), count);
            w.Write((prefix + "compare").c_str(), ch.compare);
            w.Write((prefix + "control").c_str(), ch.control);
            w.Write((prefix + "select").c_str(), ch.select);
            w.Write((prefix + "select_written").c_str(), ch.select_written);
            w.Write((prefix + "compare_written").c_str(), ch.compare_written);
            w.Write((prefix + "control_written").c_str(), ch.control_written);
        }
        timer_clock_.Save(w);
    }

    void RestoreState(StateReader& r) override {
        for (Channel& ch : ch_) {
            const std::string prefix = kChannelPrefix[Index(ch)];
            r.Read((prefix + "count").c_str(), ch.held);
            r.Read((prefix + "compare").c_str(), ch.compare);
            r.Read((prefix + "control").c_str(), ch.control);
            r.Read((prefix + "select").c_str(), ch.select);
            r.Read((prefix + "select_written").c_str(), ch.select_written);
            r.Read((prefix + "compare_written").c_str(), ch.compare_written);
            r.Read((prefix + "control_written").c_str(), ch.control_written);
        }
        timer_clock_.Restore(r);
    }

    void PostRestore() override {
        timer_clock_.PostRestore();
        for (Channel& ch : ch_) {
            if (Running(ch)) ch.counter.Anchor(timer_clock_.Ticks(ch.select), ch.held);
            Update(ch);
        }
    }

private:
    struct Channel {
        TickAnchoredCounter     counter;
        GuestCycleClock::Event* event           = nullptr;
        uint32_t                held            = 0u;
        uint32_t                compare         = 0u;
        uint16_t                control         = kStop;
        uint16_t                select          = 0u;
        bool                    select_written  = false;
        bool                    compare_written = false;
        bool                    control_written = false;
    };

    static uint32_t Offset(uint32_t addr) { return (addr - kBase) % kChannelStride; }

    Channel& ChannelOf(uint32_t addr) { return ch_[(addr - kBase) / kChannelStride]; }

    uint32_t Index(const Channel& ch) const { return static_cast<uint32_t>(&ch - ch_); }

    static bool Running(const Channel& ch) { return (ch.control & kRun) != 0u; }

    uint32_t Count(const Channel& ch) {
        if (!Running(ch)) return ch.held;
        return ch.counter.CountAt(timer_clock_.Ticks(ch.select));
    }

    void WriteClear(Channel& ch, uint32_t addr, uint16_t value) {
        if (value == kNoClear) return;
        if (value != kClearCount) HaltUnsupportedAccess("WriteHalf", addr, value);
        if (Running(ch)) {
            ch.counter.Anchor(timer_clock_.Ticks(ch.select), 0u);
        } else {
            ch.held = 0u;
        }
        Update(ch);
    }

    void WriteControl(Channel& ch, uint32_t addr, uint16_t value) {
        if (value != kStop && value != kRun && value != kRunInterrupt) HaltUnsupportedAccess("WriteHalf", addr, value);
        const bool run = (value & kRun) != 0u;
        if (run && !ch.select_written) {
            emu_.Get<Fatal>().Die("Hd6417096CompareTimer: channel %u starts with control 0x%04X before a clock select",
                                  Index(ch), value);
        }
        if (value == kRunInterrupt && !ch.compare_written) {
            emu_.Get<Fatal>().Die("Hd6417096CompareTimer: channel %u enables its compare interrupt before a compare value",
                                  Index(ch));
        }
        if (run && !Running(ch)) ch.counter.Anchor(timer_clock_.Ticks(ch.select), ch.held);
        if (!run && Running(ch)) ch.held = Count(ch);
        ch.control         = value;
        ch.control_written = true;
        Update(ch);
    }

    void WriteSelect(Channel& ch, uint32_t addr, uint16_t value) {
        if (!Sh77xxTimerClock::HasSource(value)) HaltUnsupportedAccess("WriteHalf", addr, value);
        if (Running(ch) && value != ch.select) {
            emu_.Get<Fatal>().Die("Hd6417096CompareTimer: channel %u select change %u -> %u while it counts", Index(ch),
                                  ch.select, value);
        }
        ch.select         = value;
        ch.select_written = true;
        Update(ch);
    }

    void Update(Channel& ch) {
        const Sh77xxIntcSource source = kCompareSource[Index(ch)];
        if (ch.control != kRunInterrupt) {
            clock_->Disarm(ch.event);
            intc_->SetSourceRequest(source, false);
            return;
        }
        const uint64_t now     = timer_clock_.Ticks(ch.select);
        const uint32_t count   = ch.counter.CountAt(now);
        const bool     matched = count >= ch.compare;
        intc_->SetSourceRequest(source, matched);
        const uint64_t until = matched ? kCounterSpan - count : static_cast<uint64_t>(ch.compare - count);
        timer_clock_.ArmAt(ch.event, now + until, ch.select);
    }

    void UpdateAll() {
        for (Channel& ch : ch_) Update(ch);
    }

    void EnterStandby() {
        for (const Channel& ch : ch_) {
            if (!Running(ch) || !Sh77xxTimerClock::Rtcclk(ch.select)) continue;
            emu_.Get<Fatal>().Die("Hd6417096CompareTimer: standby entered while channel %u counts with select %u",
                                  Index(ch), ch.select);
        }
    }

    void Reset() {
        for (Channel& ch : ch_) {
            ch.held            = 0u;
            ch.compare         = 0u;
            ch.control         = kStop;
            ch.select          = 0u;
            ch.select_written  = false;
            ch.compare_written = false;
            ch.control_written = false;
            Update(ch);
        }
    }

    GuestCycleClock* clock_ = nullptr;
    Sh77xxIntc*      intc_  = nullptr;
    Sh77xxTimerClock timer_clock_{emu_};
    Channel          ch_[kChannels];
};

}

REGISTER_SERVICE(Hd6417096CompareTimer);

}

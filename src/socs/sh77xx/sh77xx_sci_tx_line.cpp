#include "sh77xx_sci_tx_line.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/serial/serial_line.h"
#include "../../peripherals/serial/uart_frame.h"
#include "sh77xx_cpg.h"
#include "sh77xx_frqcr.h"
#include "sh77xx_sci_channel.h"
#include "sh77xx_serial_format.h"

#include <utility>

namespace cerfcore {

namespace {

constexpr uint8_t kInitialData  = 0xFFu;
constexpr uint8_t kEmpty        = kSh77xxSciTdre | kSh77xxSciTend;
constexpr uint8_t kSevenBitData = 0x7Fu;

}

void Sh77xxSciTxLine::Attach(std::function<void()> on_due) {
    clock_ = &emu_.Get<GuestCycleClock>();
    cpg_   = &emu_.Get<Sh77xxCpg>();
    frqcr_ = &emu_.Get<Sh77xxFrqcr>();
    event_ = clock_->Add(std::move(on_due));
}

void Sh77xxSciTxLine::Initialize() {
    Stop();
    tdr_       = kInitialData;
    flags_     = kEmpty;
    empty_irq_ = false;
    end_irq_   = false;
    sent_.clear();
}

uint8_t Sh77xxSciTxLine::Data() const { return tdr_; }

uint8_t Sh77xxSciTxLine::Flags() const { return flags_; }

void Sh77xxSciTxLine::Write(uint8_t value) { tdr_ = value; }

bool Sh77xxSciTxLine::ClearEmpty(uint8_t smr, uint8_t brr, bool enabled) {
    if (!enabled) return true;
    const uint64_t now = clock_->Cycles();
    Shape(smr, brr);
    flags_ = static_cast<uint8_t>(flags_ & ~kEmpty);
    if (shifting_) {
        pending_   = true;
        load_tick_ = stop_tick_;
        Arm();
        return true;
    }
    if (timed_ && bits_.TicksAt(now) < free_tick_) {
        Load(free_tick_);
    } else {
        if (!bits_.SetRate(clock_->ClockRate(), bit_rate_)) return false;
        bits_.Start(now);
        timed_ = true;
        Load(0u);
    }
    Arm();
    return true;
}

uint8_t Sh77xxSciTxLine::Disable() {
    Stop();
    const uint8_t risen = static_cast<uint8_t>(kEmpty & ~flags_);
    flags_              = static_cast<uint8_t>(flags_ | kEmpty);
    return risen;
}

uint8_t Sh77xxSciTxLine::Settle() {
    const uint64_t now   = clock_->Cycles();
    uint8_t        risen = 0u;
    for (;;) {
        if (pending_ && bits_.CycleOfTick(load_tick_) <= now) {
            pending_ = false;
            Load(load_tick_ + stop_bits_);
            risen = static_cast<uint8_t>(risen | kSh77xxSciTdre);
            continue;
        }
        if (!pending_ && shifting_ && bits_.CycleOfTick(stop_tick_) <= now) {
            shifting_  = false;
            free_tick_ = stop_tick_ + stop_bits_;
            flags_     = static_cast<uint8_t>(flags_ | kSh77xxSciTend);
            risen      = static_cast<uint8_t>(risen | kSh77xxSciTend);
            continue;
        }
        break;
    }
    Arm();
    return risen;
}

void Sh77xxSciTxLine::TakeSent(std::vector<uint8_t>& out) { out.swap(sent_); }

void Sh77xxSciTxLine::SetInterrupts(bool empty_irq, bool end_irq) {
    empty_irq_ = empty_irq;
    end_irq_   = end_irq;
    Arm();
}

bool Sh77xxSciTxLine::Reformat(uint8_t smr, uint8_t brr) {
    const GuestCycleClock::Rate before = bit_rate_;
    Shape(smr, brr);
    if (timed_ && bit_rate_ != before && !bits_.Rescale(clock_->Cycles(), clock_->ClockRate(), bit_rate_)) return false;
    Arm();
    return true;
}

bool Sh77xxSciTxLine::Rescale() {
    if (timed_ && !bits_.Rescale(clock_->Cycles(), clock_->ClockRate(), bit_rate_)) return false;
    Arm();
    return true;
}

void Sh77xxSciTxLine::Save(StateWriter& w) const {
    const TickPosition pos = timed_ ? bits_.PositionAt(clock_->Cycles()) : TickPosition{};
    w.Write("tx_data", tdr_);
    w.Write("tx_flags", flags_);
    w.Write("tx_seven_bit", seven_bit_);
    w.Write("tx_timed", timed_);
    w.Write("tx_bit_ticks", pos.ticks);
    w.Write("tx_bit_phase", pos.phase);
    w.Write("tx_bit_phase_den", pos.phase_den);
    w.Write("tx_bit_rate_num", bit_rate_.num);
    w.Write("tx_bit_rate_den", bit_rate_.den);
    w.Write("tx_pending", pending_);
    w.Write("tx_shifting", shifting_);
    w.Write("tx_load_tick", load_tick_);
    w.Write("tx_stop_tick", stop_tick_);
    w.Write("tx_free_tick", free_tick_);
    w.Write("tx_to_stop", to_stop_);
    w.Write("tx_stop_bits", stop_bits_);
}

bool Sh77xxSciTxLine::Restore(StateReader& r) {
    TickPosition pos;
    r.Read("tx_data", tdr_);
    r.Read("tx_flags", flags_);
    r.Read("tx_seven_bit", seven_bit_);
    r.Read("tx_timed", timed_);
    r.Read("tx_bit_ticks", pos.ticks);
    r.Read("tx_bit_phase", pos.phase);
    r.Read("tx_bit_phase_den", pos.phase_den);
    r.Read("tx_bit_rate_num", bit_rate_.num);
    r.Read("tx_bit_rate_den", bit_rate_.den);
    r.Read("tx_pending", pending_);
    r.Read("tx_shifting", shifting_);
    r.Read("tx_load_tick", load_tick_);
    r.Read("tx_stop_tick", stop_tick_);
    r.Read("tx_free_tick", free_tick_);
    r.Read("tx_to_stop", to_stop_);
    r.Read("tx_stop_bits", stop_bits_);
    sent_.clear();
    if (!timed_) return true;
    if (!bits_.SetRate(clock_->ClockRate(), bit_rate_)) return false;
    bits_.PlaceAt(clock_->Cycles(), pos);
    return true;
}

void Sh77xxSciTxLine::Load(uint64_t frame_start) {
    shifting_  = true;
    stop_tick_ = frame_start + to_stop_;
    flags_     = static_cast<uint8_t>(flags_ | kSh77xxSciTdre);
    sent_.push_back(seven_bit_ ? static_cast<uint8_t>(tdr_ & kSevenBitData) : tdr_);
}

void Sh77xxSciTxLine::Shape(uint8_t smr, uint8_t brr) {
    const SerialLine::LineConfig format = Sh77xxSerialFrameFormat(smr);
    seven_bit_                          = format.data_bits == 7u;
    to_stop_                            = UartBitsBeforeStop(format);
    stop_bits_                          = UartFrameBits(format) - to_stop_;
    bit_rate_                           = Sh77xxSerialBitRate(smr, brr, cpg_->PeripheralRate(frqcr_->Value()));
}

void Sh77xxSciTxLine::Stop() {
    timed_     = false;
    pending_   = false;
    shifting_  = false;
    free_tick_ = 0u;
    clock_->Disarm(event_);
}

void Sh77xxSciTxLine::Arm() {
    if (pending_ && empty_irq_) {
        clock_->Arm(event_, bits_.CycleOfTick(load_tick_));
        return;
    }
    if (pending_ && end_irq_) {
        clock_->Arm(event_, bits_.CycleOfTick(load_tick_ + stop_bits_ + to_stop_));
        return;
    }
    if (!pending_ && shifting_ && end_irq_) {
        clock_->Arm(event_, bits_.CycleOfTick(stop_tick_));
        return;
    }
    clock_->Disarm(event_);
}

}

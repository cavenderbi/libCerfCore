#include "rcvdl56dpfl_data_pump_peripheral.h"

#include "rcvdl56dpfl_bit_tick.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

void Rcvdl56dpflDataPumpPeripheral::StartDataPump() {
    clock_ = &emu_.Get<GuestCycleClock>();
    event_ = clock_->Add([this] { pump_.TimerElapsed(); });
    clock_->RegisterRateListener([this] { RescaleBitClock(); });
}

uint8_t Rcvdl56dpflDataPumpPeripheral::ReadPumpRegister(uint32_t reg) {
    return pump_.Read(reg);
}

void Rcvdl56dpflDataPumpPeripheral::WritePumpRegister(uint32_t reg, uint8_t value) {
    if (const char* refused = pump_.Write(reg, value))
        emu_.Get<Fatal>().Die("[MDP] register %02Xh write 0x%02X: %s", reg, value, refused);
}

void Rcvdl56dpflDataPumpPeripheral::SetResetPin(bool high) {
    pump_.SetResetPin(high);
}

void Rcvdl56dpflDataPumpPeripheral::SaveState(StateWriter& w) {
    pump_.SaveState(w);
    const TickPosition pos = timed_ ? bit_ticks_.PositionAt(clock_->Cycles()) : TickPosition{};
    w.Write<uint8_t>("bit_clock_timed", timed_ ? 1u : 0u);
    w.Write<uint64_t>("bit_clock_ticks", pos.ticks);
    w.Write<uint64_t>("bit_clock_phase", pos.phase);
    w.Write<uint64_t>("bit_clock_phase_den", pos.phase_den);
    w.Write<uint64_t>("bit_rate_num", bit_rate_.num);
    w.Write<uint64_t>("bit_rate_den", bit_rate_.den);
}

void Rcvdl56dpflDataPumpPeripheral::RestoreState(StateReader& r) {
    pump_.RestoreState(r);
    uint8_t      timed = 0u;
    TickPosition pos;
    r.Read("bit_clock_timed", timed);
    r.Read("bit_clock_ticks", pos.ticks);
    r.Read("bit_clock_phase", pos.phase);
    r.Read("bit_clock_phase_den", pos.phase_den);
    r.Read("bit_rate_num", bit_rate_.num);
    r.Read("bit_rate_den", bit_rate_.den);
    timed_ = timed != 0u;
    if (timed_ && (!bit_ticks_.SetRate(clock_->ClockRate(), bit_rate_) ||
                   !bit_ticks_.PlaceAt(clock_->Cycles(), pos)))
        r.Reject("[MDP] bit clock %llu/%llu Hz does not fit the core clock",
                 static_cast<unsigned long long>(bit_rate_.num), static_cast<unsigned long long>(bit_rate_.den));
    pump_.RearmTimer();
}

void Rcvdl56dpflDataPumpPeripheral::SetIrqLevel(bool asserted) {
    DriveIrq(asserted);
}

void Rcvdl56dpflDataPumpPeripheral::SetBitRate(uint32_t bits_per_second) {
    const GuestCycleClock::Rate bit{bits_per_second, 1u};
    const uint64_t              now = clock_->Cycles();
    if (!timed_) {
        if (!bit_ticks_.SetRate(clock_->ClockRate(), bit))
            emu_.Get<Fatal>().Die("[MDP] bit rate %u does not fit the core clock", bits_per_second);
        bit_ticks_.Start(now);
        timed_    = true;
        bit_rate_ = bit;
        return;
    }
    if (bit == bit_rate_) return;
    if (!bit_ticks_.Rescale(now, clock_->ClockRate(), bit))
        emu_.Get<Fatal>().Die("[MDP] bit rate %u does not fit the core clock", bits_per_second);
    bit_rate_ = bit;
}

uint64_t Rcvdl56dpflDataPumpPeripheral::BitTickNow() {
    if (!timed_) return 0u;
    const TickPosition pos = bit_ticks_.PositionAt(clock_->Cycles());
    return pos.ticks + (pos.phase != 0u ? 1u : 0u);
}

uint64_t Rcvdl56dpflDataPumpPeripheral::BitTicksElapsed() {
    if (!timed_) return 0u;
    return bit_ticks_.PositionAt(clock_->Cycles()).ticks;
}

void Rcvdl56dpflDataPumpPeripheral::ArmTimer(uint64_t tick) {
    if (tick == kRcvdl56dpflNoEvent) {
        clock_->Disarm(event_);
        return;
    }
    clock_->Arm(event_, bit_ticks_.CycleOfTick(tick));
}

void Rcvdl56dpflDataPumpPeripheral::RescaleBitClock() {
    if (!timed_) return;
    if (!bit_ticks_.Rescale(clock_->Cycles(), clock_->ClockRate(), bit_rate_))
        emu_.Get<Fatal>().Die("[MDP] the core rate change overflows the bit clock");
    pump_.RearmTimer();
}

}

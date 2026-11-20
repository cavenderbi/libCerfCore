#include "sh77xx_sci_rx_line.h"

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

void Sh77xxSciRxLine::Attach(std::function<void()> on_due, std::function<void()> on_host_request,
                             std::function<void()> on_rate) {
    cpg_   = &emu_.Get<Sh77xxCpg>();
    frqcr_ = &emu_.Get<Sh77xxFrqcr>();
    timed_.Attach(std::move(on_due), std::move(on_host_request), std::move(on_rate));
}

bool Sh77xxSciRxLine::Empty() const { return timed_.Line().Idle() && (flags_ & kSh77xxSciRdrf) == 0u; }

void Sh77xxSciRxLine::Initialize() {
    Stop();
    rdr_   = 0u;
    flags_ = 0u;
    armed_ = false;
}

uint8_t Sh77xxSciRxLine::Data() const { return rdr_; }

uint8_t Sh77xxSciRxLine::Flags() const { return flags_; }

void Sh77xxSciRxLine::Clear(uint8_t bits) { flags_ = static_cast<uint8_t>(flags_ & ~bits); }

void Sh77xxSciRxLine::TakeWire(uint8_t smr, uint8_t brr, bool receiving) {
    if (!receiving || timed_.Line().WireEmpty()) {
        timed_.Line().TakeWire();
        return;
    }
    timed_.Intake(TimingOf(smr, brr), timed_.Now(), armed_);
}

uint8_t Sh77xxSciRxLine::Settle() {
    uint8_t risen = 0u;
    timed_.Settle(timed_.Now(), armed_,
                  [&](uint16_t entry) { risen = static_cast<uint8_t>(risen | Land(static_cast<uint8_t>(entry))); });
    return risen;
}

void Sh77xxSciRxLine::SetArmed(bool armed) {
    armed_ = armed;
    timed_.Arm(timed_.Now(), armed_);
}

void Sh77xxSciRxLine::Reformat(uint8_t old_smr, uint8_t smr, uint8_t brr) {
    TimedUartRxLine::Timing before;
    before.frame = FrameOf(old_smr);
    timed_.Retime(before, TimingOf(smr, brr), timed_.Now(), armed_);
}

void Sh77xxSciRxLine::Rescale() { timed_.Rescale(timed_.Now(), armed_); }

void Sh77xxSciRxLine::Stop() { timed_.Stop(); }

void Sh77xxSciRxLine::Save(StateWriter& w) const {
    w.Write("rx_data", rdr_);
    w.Write("rx_flags", flags_);
    timed_.Save(w);
}

void Sh77xxSciRxLine::Restore(StateReader& r) {
    r.Read("rx_data", rdr_);
    r.Read("rx_flags", flags_);
    timed_.Restore(r);
}

UartRxLine::Frame Sh77xxSciRxLine::FrameOf(uint8_t smr) {
    const SerialLine::LineConfig format = Sh77xxSerialFrameFormat(smr);
    return UartRxLine::FirstStopCentre(UartBitsBeforeStop(format), UartFrameBits(format));
}

TimedUartRxLine::Timing Sh77xxSciRxLine::TimingOf(uint8_t smr, uint8_t brr) const {
    TimedUartRxLine::Timing t;
    t.clocked = true;
    t.bit     = Sh77xxSerialBitRate(smr, brr, cpg_->PeripheralRate(frqcr_->Value()));
    t.frame   = FrameOf(smr);
    return t;
}

uint8_t Sh77xxSciRxLine::Land(uint8_t byte) {
    if ((flags_ & kSh77xxSciOrer) != 0u) return 0u;
    if ((flags_ & kSh77xxSciRdrf) != 0u) {
        flags_ = static_cast<uint8_t>(flags_ | kSh77xxSciOrer);
        return kSh77xxSciOrer;
    }
    rdr_   = byte;
    flags_ = static_cast<uint8_t>(flags_ | kSh77xxSciRdrf);
    return kSh77xxSciRdrf;
}

}

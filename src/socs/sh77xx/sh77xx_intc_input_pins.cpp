#include "sh77xx_intc_input_pins.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

using namespace sh77xx_intc;

REGISTER_SERVICE(Sh77xxIntcInputPins);

bool Sh77xxIntcInputPins::ShouldRegister() { return emu_.Get<BoardContext>().GetCpuArch() == CpuArch::kSh; }

uint32_t Sh77xxIntcInputPins::Sense(uint16_t icr1, uint32_t n) { return (icr1 >> (2u * n)) & 3u; }

bool Sh77xxIntcInputPins::High(uint32_t n) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return HighLocked(n);
}

void Sh77xxIntcInputPins::SetNet(uint32_t bit, bool high) {
    std::lock_guard<std::mutex> lk(mtx_);
    net_high_ = high ? (net_high_ | bit) : (net_high_ & ~bit);
}

void Sh77xxIntcInputPins::SetDrive(uint32_t pins, uint32_t high) {
    std::lock_guard<std::mutex> lk(mtx_);
    drive_pins_ = pins;
    drive_high_ = high & pins;
}

bool Sh77xxIntcInputPins::Apply(uint16_t icr1, uint32_t irl_pins, bool resetting) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint32_t              levels  = Settled();
    const uint32_t              changed = levels ^ high_;
    if (changed == 0u) return false;
    for (uint32_t n = 0; n < kIrqPins; ++n) {
        if (((changed >> n) & 1u) == 0u || ((irl_pins >> n) & 1u) != 0u || resetting) continue;
        const bool     was_high = HighLocked(n);
        const uint32_t sense    = Sense(icr1, n);
        if ((sense == kSenseFalling && was_high) || (sense == kSenseRising && !was_high)) irq_latch_ |= 1u << n;
    }
    high_ = levels;
    return true;
}

bool Sh77xxIntcInputPins::IrqRequest(uint16_t icr1, uint32_t n) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return IrqRequestLocked(icr1, n);
}

uint32_t Sh77xxIntcInputPins::PintRequests(uint16_t icr2, uint16_t pinter, uint32_t override_pints,
                                           uint32_t override_high) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return PintRequestsLocked(icr2, pinter, override_pints, override_high);
}

uint32_t Sh77xxIntcInputPins::IrlCode(bool irls_in_use) const {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint32_t              irl = high_ & 0xFu;
    if (!irls_in_use) return irl;
    const uint32_t irls = (high_ & kIrlsPins) >> kIrlsBit0;
    return irls < irl ? irls : irl;
}

uint8_t Sh77xxIntcInputPins::ReadIrr0(uint16_t icr1, uint16_t icr2, uint16_t pinter, uint32_t absent_pints) {
    std::lock_guard<std::mutex> lk(mtx_);
    uint8_t                     value = 0u;
    for (uint32_t n = 0; n < kIrqPins; ++n) {
        if (IrqRequestLocked(icr1, n)) value = static_cast<uint8_t>(value | (1u << n));
    }
    const uint32_t pint = PintRequestsLocked(icr2, pinter, 0u, 0u) & ~absent_pints;
    if ((pint & kPintGroupBits[0]) != 0u) value |= kIrr0Pint0R;
    if ((pint & kPintGroupBits[1]) != 0u) value |= kIrr0Pint1R;
    irr0_read_ones_ |= value & kIrr0IrqMask;
    return value;
}

void Sh77xxIntcInputPins::ClearIrr0(uint8_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint32_t              cleared = ~static_cast<uint32_t>(value) & irr0_read_ones_ & kIrr0IrqMask;
    irq_latch_ &= ~cleared;
    irr0_read_ones_ &= ~cleared;
}

void Sh77xxIntcInputPins::ResetLatches() {
    std::lock_guard<std::mutex> lk(mtx_);
    irq_latch_      = 0u;
    irr0_read_ones_ = 0u;
}

void Sh77xxIntcInputPins::SaveState(StateWriter& w) const {
    std::lock_guard<std::mutex> lk(mtx_);
    w.Write("pin_net_high", net_high_);
    w.Write("pin_drive_pins", drive_pins_);
    w.Write("pin_drive_high", drive_high_);
    w.Write("irq_latch", irq_latch_);
    w.Write("irr0_read_ones", irr0_read_ones_);
}

void Sh77xxIntcInputPins::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(mtx_);
    r.Read("pin_net_high", net_high_);
    r.Read("pin_drive_pins", drive_pins_);
    r.Read("pin_drive_high", drive_high_);
    r.Read("irq_latch", irq_latch_);
    r.Read("irr0_read_ones", irr0_read_ones_);
    high_ = Settled();
}

bool Sh77xxIntcInputPins::HighLocked(uint32_t n) const { return ((high_ >> n) & 1u) != 0u; }

bool Sh77xxIntcInputPins::IrqRequestLocked(uint16_t icr1, uint32_t n) const {
    if (Sense(icr1, n) == kSenseLow) return !HighLocked(n);
    return ((irq_latch_ >> n) & 1u) != 0u;
}

uint32_t Sh77xxIntcInputPins::PintRequestsLocked(uint16_t icr2, uint16_t pinter, uint32_t override_pints,
                                                 uint32_t override_high) const {
    const uint32_t levels =
        (((high_ >> kPintBit0) & ~override_pints) | (override_high & override_pints)) & kPintPinMask;
    return ~(levels ^ icr2) & pinter & kPintPinMask;
}

uint32_t Sh77xxIntcInputPins::Settled() const { return (net_high_ & ~drive_pins_) | drive_high_; }

}

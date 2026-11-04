#include "vr4121_clock_state.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../vr41xx/vr41xx_clock_strap.h"
#include "vr4121_core_clock_model.h"
#include "vr4121_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kDivMask    = 0x000Fu;
constexpr uint16_t kDivtShift  = 12u;
constexpr uint16_t kDivvtShift = 8u;

}

REGISTER_SERVICE(Vr4121ClockState);

bool Vr4121ClockState::ShouldRegister() {
    return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4121;
}

void Vr4121ClockState::OnReady() {
    Vr41xxClockState::OnReady();
    row_ = &emu_.Get<Vr41xxClockStrap>().StrapRow(kVr4121CoreClock.rows);
}

const char* Vr4121ClockState::RejectedPending(uint16_t value) const {
    if ((value & ~kDivMask) != 0u) return "PMUDIVREG WriteHalf sets a write-0 RFU bit";
    if (((row_->pmu_div_modes >> value) & 1u) == 0u) {
        return "PMUDIVREG WriteHalf with a Div mode this CLKSEL does not allow";
    }
    return nullptr;
}

uint16_t Vr4121ClockState::ClkSpeedReg() const {
    const uint16_t divt  = static_cast<uint16_t>(Vr41xxPclockPerTclock(kVr4121CoreClock, *row_));
    const uint16_t divvt = Active() != 0u ? Active() : divt;
    return static_cast<uint16_t>((divt << kDivtShift) | (divvt << kDivvtShift) | row_->clksp);
}

}

#pragma once

#include "../vr41xx/vr41xx_clock_state.h"
#include "../vr41xx/vr41xx_core_clock_impl.h"

#include <cstdint>

namespace cerfcore {

class Vr4121ClockState : public Vr41xxClockState {
public:
    using Vr41xxClockState::Vr41xxClockState;

    bool ShouldRegister() override;
    void OnReady() override;

    const Vr41xxClkselRow& Row() const { return *row_; }
    uint16_t               ClkSpeedReg() const;

protected:
    const char* RejectedPending(uint16_t value) const override;

private:
    const Vr41xxClkselRow* row_ = nullptr;
};

}

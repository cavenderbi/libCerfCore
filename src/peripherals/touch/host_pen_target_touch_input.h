#pragma once

#include "host_pen_target.h"
#include "host_pen_touch_input.h"

namespace cerfcore {

class HostPenTargetTouchInput : public HostPenTouchInput {
public:
    using HostPenTouchInput::HostPenTouchInput;

protected:
    virtual HostPenTarget& Target() = 0;

    void OnTouchReady() final;
    void ApplyHostPen(const HostPen& pen) final { target_->ApplyPen(pen); }

private:
    HostPenTarget* target_ = nullptr;
};

}

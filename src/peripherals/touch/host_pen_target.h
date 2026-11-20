#pragma once

#include "host_pen_input.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

class HostPenTarget {
public:
    enum class Plate { kX, kY };

    using PositionSource = std::function<void(const HostPenInput::PanelRange&, uint16_t*, uint16_t*)>;
    using PenQueue       = std::function<void(const HostPenInput::Pen&)>;

    void AttachTouch(PositionSource position, PenQueue queue);

    virtual void ApplyPen(const HostPenInput::Pen& pen) = 0;
    virtual void ResetPen() = 0;

protected:
    ~HostPenTarget() = default;

    uint16_t PlatePosition(const HostPenInput::PanelRange& range, Plate plate) const;
    void     QueuePen(const HostPenInput::Pen& pen) const { queue_(pen); }

private:
    PositionSource position_;
    PenQueue       queue_;
};

}

#pragma once

#include <cerfcore/host/touch_input.h>
#include "host_pen_input.h"

#include <cstdint>

namespace cerfcore {

class HostPenTouchInput : public TouchInput {
public:
    using TouchInput::TouchInput;

    using PanelRange = HostPenInput::PanelRange;

    void OnReady() override;

protected:
    void PenDown(int x, int y) final;
    void PenMove(int x, int y) final;
    void PenUp(int x, int y) final;
    void CaptureLost() final;

    using HostPen = HostPenInput::Pen;
    static constexpr uint8_t kPenUp          = HostPenInput::kPenUp;
    static constexpr uint8_t kPenDown        = HostPenInput::kPenDown;
    static constexpr uint8_t kFirstOwnerKind = HostPenInput::kFirstOwnerKind;

    virtual void OnTouchReady() = 0;
    virtual void ApplyHostPen(const HostPen& pen) = 0;

    void QueueHostPen(const HostPen& pen) { pens_.Queue(pen); }
    void LatestPosition(int* x, int* y) {
        const HostPenInput::Point p = pens_.Latest();
        *x = p.x;
        *y = p.y;
    }
    void MapToPanel(int x, int y, const PanelRange& range, uint16_t* adc_x, uint16_t* adc_y) const {
        pens_.MapToPanel(x, y, range, adc_x, adc_y);
    }
    void LatestOnPanel(const PanelRange& range, uint16_t* adc_x, uint16_t* adc_y) const {
        const HostPenInput::Point p = pens_.Latest();
        pens_.MapToPanel(p.x, p.y, range, adc_x, adc_y);
    }
    void ApplyPensInPark() { pens_.ApplyInPark(); }
    void ClearPens() { pens_.Clear(); }

private:
    HostPenInput pens_;
};

}

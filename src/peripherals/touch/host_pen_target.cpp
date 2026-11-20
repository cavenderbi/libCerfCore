#include "host_pen_target.h"

#include <utility>

namespace cerfcore {

void HostPenTarget::AttachTouch(PositionSource position, PenQueue queue) {
    position_ = std::move(position);
    queue_    = std::move(queue);
}

uint16_t HostPenTarget::PlatePosition(const HostPenInput::PanelRange& range, Plate plate) const {
    uint16_t adc_x = 0u;
    uint16_t adc_y = 0u;
    position_(range, &adc_x, &adc_y);
    return plate == Plate::kX ? adc_x : adc_y;
}

}

#include "host_pen_target_touch_input.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/hibernation.h>

namespace cerfcore {

void HostPenTargetTouchInput::OnTouchReady() {
    target_ = &Target();
    target_->AttachTouch(
        [this](const PanelRange& range, uint16_t* adc_x, uint16_t* adc_y) { LatestOnPanel(range, adc_x, adc_y); },
        [this](const HostPen& pen) { QueueHostPen(pen); });
    ApplyPensInPark();
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] {
        ClearPens();
        target_->ResetPen();
    });
}

}

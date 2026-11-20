#pragma once

#include "../../socs/guest_sample_clock.h"
#include "../sh77xx_pen_line_panel.h"

#include <cstdint>

namespace cerfcore {

class Hp600TouchPanel : public Sh77xxPenLinePanel {
public:
    using Sh77xxPenLinePanel::Sh77xxPenLinePanel;

    bool ShouldRegister() override;
    void OnReady() override;

    void ApplyPen(const HostPenInput::Pen& pen) override;
    void ResetPen() override;

    void TapRaw(uint16_t adc_x, uint16_t adc_y);

    uint16_t ReadPlate(Plate plate);

protected:
    Sh77xxIntcPin PenLine() const override { return Sh77xxIntcPin::kIrq3; }

private:
    GuestSampleClock raw_hold_{emu_};
    bool             raw_tap_ = false;
    uint16_t         raw_x_   = 0u;
    uint16_t         raw_y_   = 0u;
};

}

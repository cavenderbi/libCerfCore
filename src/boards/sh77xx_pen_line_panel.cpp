#include "sh77xx_pen_line_panel.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../socs/sh77xx/sh77xx_port_data.h"

namespace cerfcore {

void Sh77xxPenLinePanel::OnReady() {
    intc_  = &emu_.Get<Sh77xxIntc>();
    ports_ = &emu_.Get<Sh77xxPortData>();
    DrivePenLine(false);
}

void Sh77xxPenLinePanel::ApplyPen(const HostPenInput::Pen& pen) { DrivePenLine(pen.kind == HostPenInput::kPenDown); }

void Sh77xxPenLinePanel::ResetPen() { DrivePenLine(false); }

void Sh77xxPenLinePanel::DrivePenLine(bool pen_down) {
    pen_down_ = pen_down;
    intc_->SetPinLevel(PenLine(), pen_down);
}

}

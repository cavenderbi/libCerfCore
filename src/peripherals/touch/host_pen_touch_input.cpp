#include "host_pen_touch_input.h"

#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

void HostPenTouchInput::OnReady() {
    pens_.Attach(emu_, [this](const HostPen& pen) { ApplyHostPen(pen); });
    OnTouchReady();
}

void HostPenTouchInput::PenDown(int x, int y) { pens_.Down(x, y); }

void HostPenTouchInput::PenMove(int x, int y) { pens_.Track(x, y); }

void HostPenTouchInput::PenUp(int, int) { pens_.Up(); }

void HostPenTouchInput::CaptureLost() { pens_.Up(); }

}

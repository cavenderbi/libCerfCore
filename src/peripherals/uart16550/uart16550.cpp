#include "uart16550.h"

namespace cerfcore {

void Uart16550::OnReady() {
    AttachLineTimer();
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) { OnLineReset(kind); });
    emu_.Get<PeripheralDispatcher>().Register(this);
}

void Uart16550::OnLineReset(ResetLineKind kind) { LineReset(kind); }

void Uart16550::SaveState(StateWriter& w) {
    Port().SaveCradleState(w);
    Serial16550::SaveState(w);
    SaveLineTimer(w, "line_timer_");
}

void Uart16550::RestoreState(StateReader& r) {
    emu_.Get<KernelDebugSink>().FlushLine(tx_line_, Name());
    Port().RestoreCradleState(r);
    Serial16550::RestoreState(r);
    RestoreLineTimer(r, "line_timer_");
}

void Uart16550::PostRestore() {
    ResumeLineTimer();
    PostRestoreLine();
}

void Uart16550::DeliverTx(uint8_t byte) {
    if (emu_.Get<KernelDebugSink>().DeliverOrEmit(Port(), &byte, 1, true, tx_line_, Name())) Port().MarkTx();
}

void Uart16550::OnShutdown() {
    Port().OnShutdown();
    emu_.Get<KernelDebugSink>().FlushLine(tx_line_, Name());
}

void Uart16550::EmitDebugTx(uint8_t byte) {
    emu_.Get<KernelDebugSink>().EmitChar(static_cast<char>(byte), tx_line_, Name());
}

}

#include "ite_it8368_socket_host.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/host_widget_registry.h>
#include "../../socs/pr31x00/pr31x00_card_space.h"

namespace cerfcore {

IteIt8368SocketHost::IteIt8368SocketHost(CerfEmulator& emu, const char* slot_name)
    : Service(emu), slot0_(emu, *this, slot_name) {}

void IteIt8368SocketHost::OnReady() {
    emu_.Get<Pr31x00CardSpace>().ProvideSockets(&slot0_, nullptr);
    emu_.Get<IteIt8368>().SetSlot(&slot0_);
    emu_.Get<IteIt8368>().SetIntSink(this);
    emu_.Get<HostWidgetRegistry>().Register(&slot0_);
}

void IteIt8368SocketHost::OnShutdown() { slot0_.OnShutdown(); }

void IteIt8368SocketHost::OnCardDetectChanged(PcmciaSlot&) {
    emu_.Get<IteIt8368>().NotifyCardDetect();
}

void IteIt8368SocketHost::OnReadyIreqLow(PcmciaSlot&) {
    emu_.Get<IteIt8368>().SetReadyIreqLow(true);
}

void IteIt8368SocketHost::OnReadyIreqHigh(PcmciaSlot&) {
    emu_.Get<IteIt8368>().SetReadyIreqLow(false);
}

}

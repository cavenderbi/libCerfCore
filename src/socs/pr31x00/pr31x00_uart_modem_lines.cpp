#include "pr31x00_uart_modem_lines.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>
#include "pr31500_id.h"
#include "pr31700_id.h"
#include "pr31x00_io.h"

#include <utility>

namespace cerfcore {

bool Pr31x00UartModemLines::ShouldRegister() {
    const std::string_view soc = emu_.Get<BoardContext>().GetSocId();
    return soc == SocId::Pr31500 || soc == SocId::Pr31700;
}

uint32_t Pr31x00UartModemLines::Attach(const Pr31x00SerialModem* modem, ControlFn on_control) {
    if (count_ >= kPorts) emu_.Get<Fatal>().Die("Pr31x00UartModemLines: a third UART attached");
    const uint32_t port = count_;
    ch_[port] = std::make_unique<Channel>();
    ch_[port]->on_control = std::move(on_control);
    ++count_;
    if (!modem) return port;
    ch_[port]->modem = *modem;
    auto& io = emu_.Get<Pr31x00Io>();
    io.RegisterMfioOutObserver([this, port](const Pr31x00Pins& p) { OnOut(port, true, p.Low()); });
    io.RegisterIoOutObserver([this, port](const Pr31x00Pins& p) { OnOut(port, false, p.Low()); });
    return port;
}

Pr31x00UartModemLines::Channel& Pr31x00UartModemLines::Ch(uint32_t port) { return *ch_[port]; }
const Pr31x00UartModemLines::Channel& Pr31x00UartModemLines::Ch(uint32_t port) const {
    return *ch_[port];
}

/* An asserted line reads as a low pin: philips_nino_300 serial.dll sub_1861A68 (CTS on MFIODIN
   & 0x40 == 0), philips_velo_1_ce1 serial.dll sub_1EB1F04 (CTS on MFIODIN & 0x40000000 == 0,
   DCD on IODIN & 0x10 == 0). */
void Pr31x00UartModemLines::SetInputs(uint32_t port, bool cts, bool dsr, bool ri, bool dcd) {
    const Pr31x00SerialModem& m = Ch(port).modem;
    DriveInput(m.cts, cts);
    DriveInput(m.dcd, dcd);
    DriveInput(m.dsr, dsr);
    DriveInput(m.ri,  ri);
}

void Pr31x00UartModemLines::DriveInput(const Pr31x00SerialPin& p, bool asserted) {
    if (!p.Wired()) return;
    auto&          io  = emu_.Get<Pr31x00Io>();
    const uint32_t pin = static_cast<uint32_t>(p.pin);
    if (p.bank == Pr31x00SerialPin::Bank::Mfio) io.DriveMfioInput(pin, !asserted);
    else                                        io.DriveIoInput(pin, !asserted);
}

void Pr31x00UartModemLines::Control(uint32_t port, bool& dtr, bool& rts) const {
    const Channel& c = Ch(port);
    std::lock_guard<std::mutex> lk(c.mu);
    dtr = c.dtr;
    rts = c.rts;
}

bool Pr31x00UartModemLines::AssertedLocked(const Channel& c, const Pr31x00SerialPin& p) {
    if (!p.Wired()) return false;
    const uint32_t low = p.bank == Pr31x00SerialPin::Bank::Mfio ? c.mfio_low : c.io_low;
    return (low & (1u << p.pin)) != 0u;
}

void Pr31x00UartModemLines::OnOut(uint32_t port, bool mfio, uint32_t low) {
    Channel& c = Ch(port);
    bool     dtr, rts;
    {
        std::lock_guard<std::mutex> lk(c.mu);
        (mfio ? c.mfio_low : c.io_low) = low;
        dtr = AssertedLocked(c, c.modem.dtr);
        rts = AssertedLocked(c, c.modem.rts);
        if (dtr == c.dtr && rts == c.rts) return;
        c.dtr = dtr;
        c.rts = rts;
    }
    c.on_control(dtr, rts);
}

void Pr31x00UartModemLines::SaveState(uint32_t port, StateWriter& w) {
    const Channel& c = Ch(port);
    uint32_t mfio_low, io_low;
    uint8_t  dtr, rts;
    {
        std::lock_guard<std::mutex> lk(c.mu);
        mfio_low = c.mfio_low;
        io_low   = c.io_low;
        dtr      = c.dtr ? 1u : 0u;
        rts      = c.rts ? 1u : 0u;
    }
    w.Write("mfio_low", mfio_low);
    w.Write("io_low", io_low);
    w.Write("dtr", dtr);
    w.Write("rts", rts);
}

void Pr31x00UartModemLines::RestoreState(uint32_t port, StateReader& r) {
    Channel& c = Ch(port);
    std::lock_guard<std::mutex> lk(c.mu);
    r.Read("mfio_low", c.mfio_low);
    r.Read("io_low", c.io_low);
    uint8_t b = 0;
    r.Read("dtr", b); c.dtr = b != 0;
    r.Read("rts", b); c.rts = b != 0;
}

REGISTER_SERVICE(Pr31x00UartModemLines);

}

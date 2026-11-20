#include "casio_cassiopeia_a10_serial_connector.h"

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10SerialConnector);

namespace {

constexpr char kSerialPortId[]   = "serial_port";
constexpr char kSerialPortName[] = "Serial port";

constexpr uint32_t kUnits = static_cast<uint32_t>(CasioCassiopeiaA10SerialUnit::kCount);

}

CasioCassiopeiaA10SerialConnector::CasioCassiopeiaA10SerialConnector(CerfEmulator& emu)
    : CasioCassiopeiaA10MaskedChipRegister(emu), switch_(emu, kUnits) {}

void CasioCassiopeiaA10SerialConnector::OnReady() {
    CasioCassiopeiaA10MaskedChipRegister::OnReady();
    switch_.AddInputsListener([this](bool cts, bool, bool, bool dcd) { SetInputs(cts, dcd); });
    switch_.Attach({kSerialPortId, kSerialPortName});
}

void CasioCassiopeiaA10SerialConnector::OnShutdown() { switch_.OnShutdown(); }

uint16_t CasioCassiopeiaA10SerialConnector::ReadHalf(uint32_t) {
    return static_cast<uint16_t>(Value() | (Carrier() ? kCasioCassiopeiaA10LineCarrier : 0u));
}

void CasioCassiopeiaA10SerialConnector::SaveState(StateWriter& w) {
    CasioCassiopeiaA10MaskedChipRegister::SaveState(w);
    w.Write("cts", cts_.load(std::memory_order_acquire));
    w.Write("carrier", carrier_.load(std::memory_order_acquire));
    switch_.SaveSwitchState(w);
}

void CasioCassiopeiaA10SerialConnector::RestoreState(StateReader& r) {
    CasioCassiopeiaA10MaskedChipRegister::RestoreState(r);
    bool cts     = false;
    bool carrier = false;
    r.Read("cts", cts);
    r.Read("carrier", carrier);
    cts_.store(cts, std::memory_order_release);
    carrier_.store(carrier, std::memory_order_release);
    switch_.RestoreSwitchState(r);
}

void CasioCassiopeiaA10SerialConnector::PostRestore() { switch_.PostRestore(); }

MachineObject* CasioCassiopeiaA10SerialConnector::AttachLine(CasioCassiopeiaA10SerialUnit unit, SerialLine& line) {
    switch_.AttachLine(static_cast<uint32_t>(unit), line);
    return switch_.Activity();
}

void CasioCassiopeiaA10SerialConnector::Route(std::optional<CasioCassiopeiaA10SerialUnit> unit,
                                              std::optional<CasioCassiopeiaA10SerialUnit> joined) {
    switch_.Select(LineIndex(unit), LineIndex(joined));
}

std::optional<uint32_t> CasioCassiopeiaA10SerialConnector::LineIndex(std::optional<CasioCassiopeiaA10SerialUnit> unit) {
    return unit ? std::optional<uint32_t>(static_cast<uint32_t>(*unit)) : std::nullopt;
}

void CasioCassiopeiaA10SerialConnector::SetInputs(bool cts, bool carrier) {
    if (cts_.exchange(cts, std::memory_order_acq_rel) != cts) cts_listeners_.Call();
    if (carrier_.exchange(carrier, std::memory_order_acq_rel) != carrier) carrier_listeners_.Call(carrier);
}

bool CasioCassiopeiaA10SerialConnector::Cts() const { return cts_.load(std::memory_order_acquire); }

bool CasioCassiopeiaA10SerialConnector::Carrier() const { return carrier_.load(std::memory_order_acquire); }

void CasioCassiopeiaA10SerialConnector::AddCtsListener(CallbackList<>::Callback listener) {
    cts_listeners_.Add(std::move(listener));
}

void CasioCassiopeiaA10SerialConnector::AddCarrierListener(CallbackList<bool>::Callback listener) {
    carrier_listeners_.Add(std::move(listener));
}

void CasioCassiopeiaA10SerialConnector::Apply(uint16_t value) {
    CasioCassiopeiaA10MaskedChipRegister::Apply(static_cast<uint16_t>(value & kCasioCassiopeiaA10LineOutputs));
}

}

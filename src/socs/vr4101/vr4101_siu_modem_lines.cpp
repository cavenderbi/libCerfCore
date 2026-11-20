#include "vr4101_siu_modem_lines.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

void WriteFlag(StateWriter& w, const char* key, bool v) { w.Write<uint8_t>(key, v ? 1u : 0u); }

bool ReadFlag(StateReader& r, const char* key) {
    uint8_t v = 0u;
    r.Read(key, v);
    return v != 0u;
}

}

void Vr4101SiuModemLines::SetConnector(bool cts, bool dsr) {
    connector_cts_ = cts;
    connector_dsr_ = dsr;
}

uint16_t Vr4101SiuModemLines::Apply(bool powered, bool dcd) {
    const bool cts     = powered && connector_cts_;
    const bool dsr     = powered && connector_dsr_;
    uint16_t   changed = 0u;
    if (cts != cts_on_) changed |= kCtsChanged;
    if (dsr != dsr_on_) changed |= kDsrChanged;
    if (dcd != dcd_on_) changed |= kDcdChanged;
    cts_on_ = cts;
    dsr_on_ = dsr;
    dcd_on_ = dcd;
    return changed;
}

uint16_t Vr4101SiuModemLines::Rs232cInputs() const {
    uint16_t v = 0u;
    if (!cts_on_) v |= kCtsHigh;
    if (dsr_on_)  v |= kDsrOn;
    if (dcd_on_)  v |= kDcdOn;
    return v;
}

void Vr4101SiuModemLines::Save(StateWriter& w) const {
    WriteFlag(w, "siu_connector_cts", connector_cts_);
    WriteFlag(w, "siu_connector_dsr", connector_dsr_);
    WriteFlag(w, "siu_cts_level", cts_on_);
    WriteFlag(w, "siu_dsr_level", dsr_on_);
    WriteFlag(w, "siu_dcd_level", dcd_on_);
}

void Vr4101SiuModemLines::Restore(StateReader& r) {
    connector_cts_ = ReadFlag(r, "siu_connector_cts");
    connector_dsr_ = ReadFlag(r, "siu_connector_dsr");
    cts_on_        = ReadFlag(r, "siu_cts_level");
    dsr_on_        = ReadFlag(r, "siu_dsr_level");
    dcd_on_        = ReadFlag(r, "siu_dcd_level");
}

}
